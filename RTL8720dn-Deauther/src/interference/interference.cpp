#include "interference.h"
#include "wifi_conf.h"
#include "../../debug.h"
#include "../core/led_controller.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
    void wifi_enter_promisc_mode();
}

// ===================================================================
// CONSTANTS & CONFIG
// ===================================================================
static const int adj_channels_24[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
// Non-DFS UNII-1 & UNII-3 channels supported cleanly across regulatory domains
static const int adj_channels_5g[] = {36, 40, 44, 48, 149, 153, 157, 161, 165};
static const uint8_t csa_ghost_24[] = {12, 13, 1};
static const uint8_t csa_ghost_5g[] = {36, 40, 44, 48, 149, 153, 157, 161};

#define CAPTURE_TIMEOUT_MS       2500
#define CHANNEL_SWITCH_DELAY_MS  BAND_SWITCH_DELAY_MS
#define BURST_COUNT              120
#define TARGET_RATIO             9
#define MAX_BEACON_TAGS_LEN      400

// ===================================================================
// GLOBAL STATE
// ===================================================================
static BeaconClone targets[MAX_INTERFERENCE_TARGETS];
static int         num_targets    = 0;
static int         active_target  = 0;
static int         adj_hop_idx    = 0;
static int         csa_idx        = 0;
static uint32_t    g_mac_counter  = 0xA5000000;
static uint32_t    last_blink_ms  = 0;
static uint32_t    last_hb_ms     = 0;

// Lock-free single-producer (RX task) -> single-consumer (main_task) staging buffer
static volatile bool sniffer_active = false;
static volatile bool rx_captured_ready = false;
static int           rx_target_idx = -1;
static uint16_t      rx_beacon_interval = 0x64;
static uint16_t      rx_capabilities = 0x0411;
static size_t        rx_tags_len = 0;
static uint8_t       rx_tags_buf[MAX_BEACON_TAGS_LEN];

// ===================================================================
// INTERNAL HELPERS
// ===================================================================

size_t interference_sanitize_beacon_tags(const uint8_t* tags, size_t raw_len, size_t max_len) {
    if (!tags || raw_len < 2) return 0;
    if (max_len > raw_len) max_len = raw_len;

    size_t pos = 0;
    while (pos + 2 <= max_len) {
        uint8_t ie_len = tags[pos + 1];
        size_t total_ie = 2U + (size_t)ie_len;
        if (pos + total_ie > max_len) {
            break;
        }
        pos += total_ie;
    }
    return pos;
}

static void _build_synthetic_payload(BeaconClone* t) {
    if (!t) return;
    uint8_t ch = (uint8_t)t->target_channel;
    bool is5GHz = (t->target_channel > 14);

    if (t->payload) {
        free(t->payload);
        t->payload = NULL;
    }
    t->payload_len = 0;

    if (is5GHz) {
        static const uint8_t tpl_5g[] = {
            0x00, 0x00,                                         // Tag 0: Hidden SSID
            0x01, 0x08, 0x8C, 0x12, 0x98, 0x24,
                        0xB0, 0x48, 0x60, 0x6C,                 // Tag 1: Supported Rates (6-54M)
            0x2D, 0x1A,
            0xEF, 0x09, 0x17,
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,           // Tag 45: HT Cap
            0x3D, 0x16,                                         // Tag 61: HT Operation
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const size_t tpl_len = sizeof(tpl_5g);
        t->payload = (uint8_t*)malloc(tpl_len);
        if (!t->payload) {
            t->state = INT_STATE_IDLE;
            return;
        }
        memcpy(t->payload, tpl_5g, tpl_len);
        // Primary channel byte inside Tag 61 (HT Operation): 2 + 10 + 28 + 2 = offset 42
        t->payload[42] = ch;
        t->payload_len = tpl_len;
    } else {
        static const uint8_t tpl_24[] = {
            0x00, 0x00,                                         // Tag 0: Hidden SSID
            0x01, 0x08, 0x82, 0x84, 0x8B, 0x96,
                        0x0C, 0x12, 0x18, 0x24,                 // Tag 1: 1,2,5.5,11,6,9,12,18M
            0x03, 0x01, 0x00,                                   // Tag 3: DS Param (ch filled)
            0x05, 0x04, 0x00, 0x01, 0x00, 0x00,                 // Tag 5: TIM
            0x2A, 0x01, 0x00,                                   // Tag 42: ERP Info
            0x32, 0x04, 0x30, 0x48, 0x60, 0x6C                  // Tag 50: 24,36,48,54M
        };
        const size_t tpl_len = sizeof(tpl_24);
        t->payload = (uint8_t*)malloc(tpl_len);
        if (!t->payload) {
            t->state = INT_STATE_IDLE;
            return;
        }
        memcpy(t->payload, tpl_24, tpl_len);
        t->payload[14] = ch;
        t->payload_len = tpl_len;
    }
    t->state = INT_STATE_ACTIVE;
}

static void _activate_synthetic(BeaconClone* t) {
    if (!t) return;
    sniffer_active = false;
    rx_captured_ready = false;
    wifi_set_promisc((rtw_rcr_level_t)0, NULL, 0);
    _build_synthetic_payload(t);
    if (t->payload) {
        DEBUG_SER_PRINT("[!] INT: Synthetic fallback for CH");
        DEBUG_SER_PRINTLN(t->target_channel);
        ledSetGreen(false);
    }
}

// ===================================================================
// SNIFFER CALLBACK (WLAN RX thread context)
// Must NEVER call wifi_set_promisc() (would self-deadlock promisc_sema),
// malloc/free, Serial, or Arduino GPIO APIs.
// ===================================================================
static void _beacon_sniffer_cb(unsigned char* frame, unsigned int len, void* arg) {
    (void)arg;
    if (!sniffer_active || rx_captured_ready || !frame || len < 38) return;

    // Require 802.11 Beacon Frame Control: Type=Mgmt(0), Subtype=Beacon(8) => 0x80,
    // ToDS=0 & FromDS=0 in byte 1.
    if (frame[0] != 0x80 || (frame[1] & 0x03) != 0x00) return;

    const int idx = rx_target_idx;
    if (idx < 0 || idx >= num_targets || idx >= MAX_INTERFERENCE_TARGETS) return;
    BeaconClone* t = &targets[idx];
    if (t->state != INT_STATE_CAPTURING) return;

    if (memcmp(&frame[16], t->bssid, 6) != 0) return;

    size_t valid_len = interference_sanitize_beacon_tags(&frame[36], (size_t)(len - 36), MAX_BEACON_TAGS_LEN);
    if (valid_len == 0) return;

    memcpy(&rx_beacon_interval, &frame[32], 2);
    memcpy(&rx_capabilities, &frame[34], 2);
    memcpy(rx_tags_buf, &frame[36], valid_len);
    rx_tags_len = valid_len;

    sniffer_active = false;
    rx_captured_ready = true;
}

static void _finalize_captured_beacon() {
    if (!rx_captured_ready) return;

    const int idx = rx_target_idx;
    rx_captured_ready = false;
    wifi_set_promisc((rtw_rcr_level_t)0, NULL, 0);

    if (idx < 0 || idx >= num_targets || idx >= MAX_INTERFERENCE_TARGETS) return;
    BeaconClone* t = &targets[idx];

    t->beacon_interval = rx_beacon_interval;
    t->capabilities = rx_capabilities;
    if (t->payload) {
        free(t->payload);
        t->payload = NULL;
    }
    t->payload_len = 0;

    if (rx_tags_len > 0 && rx_tags_len <= MAX_BEACON_TAGS_LEN) {
        t->payload = (uint8_t*)malloc(rx_tags_len);
        if (t->payload) {
            memcpy(t->payload, rx_tags_buf, rx_tags_len);
            t->payload_len = rx_tags_len;
            t->state = INT_STATE_ACTIVE;
            DEBUG_SER_PRINT("[+] INT: REAL beacon captured! CH");
            DEBUG_SER_PRINT(t->target_channel);
            DEBUG_SER_PRINT(" Tags=");
            DEBUG_SER_PRINT((unsigned long)rx_tags_len);
            DEBUG_SER_PRINTLN("B");
            ledSetGreen(false);
            return;
        }
    }
    t->state = INT_STATE_IDLE;
    ledSetGreen(false);
}

static void _start_sniffer_for(int idx) {
    if (idx < 0 || idx >= MAX_INTERFERENCE_TARGETS) return;
    BeaconClone* t = &targets[idx];
    sniffer_active = false;
    rx_captured_ready = false;
    rx_target_idx = idx;
    wifi_set_promisc((rtw_rcr_level_t)0, NULL, 0);
    delay(30);
    wifi_enter_promisc_mode();
    delay(80);
    wext_set_channel(WLAN0_NAME, t->target_channel);
    delay(CHANNEL_SWITCH_DELAY_MS * 4);
    sniffer_active = true;
    // RTW_PROMISC_ENABLE_2 (3) delivers all 802.11 management/data frames with length
    wifi_set_promisc((rtw_rcr_level_t)3, _beacon_sniffer_cb, 1);
    DEBUG_SER_PRINT("[*] INT: Sniffer on CH");
    DEBUG_SER_PRINTLN(t->target_channel);
}

static void _init_target(BeaconClone* t, const uint8_t* bssid, int channel) {
    if (t->payload) {
        free(t->payload);
        t->payload = NULL;
    }
    memcpy(t->bssid, bssid, 6);
    t->target_channel = channel;
    t->payload_len = 0;
    t->start_time = millis();
    t->state = INT_STATE_CAPTURING;
    t->beacon_interval = 0x64;
    t->capabilities = 0x0411;
    t->cycle_count = 0;
}

static void _reset_all_targets() {
    sniffer_active = false;
    rx_captured_ready = false;
    rx_target_idx = -1;
    wifi_set_promisc((rtw_rcr_level_t)0, NULL, 0);
    for (int i = 0; i < MAX_INTERFERENCE_TARGETS; i++) {
        targets[i].state = INT_STATE_IDLE;
        if (targets[i].payload) {
            free(targets[i].payload);
            targets[i].payload = NULL;
        }
        targets[i].payload_len = 0;
        targets[i].cycle_count = 0;
    }
    last_blink_ms = 0;
    last_hb_ms = 0;
}

void start_interference(uint8_t* target_bssid, int channel) {
    if (!target_bssid || channel <= 0) return;
    _reset_all_targets();
    num_targets = 1;
    active_target = 0;
    adj_hop_idx = 0;
    csa_idx = 0;

    _init_target(&targets[0], target_bssid, channel);
    DEBUG_SER_PRINT("[*] INT[0]: BSSID=");
    debugPrintMac(target_bssid);
    DEBUG_SER_PRINT(" CH=");
    DEBUG_SER_PRINTLN(channel);

    _start_sniffer_for(0);
    ledSetGreen(true);
}
    for (int i = 0; i < num_targets; i++) {
        if (targets[i].state == INT_STATE_CAPTURING) return INT_STATE_CAPTURING;
    }
    for (int i = 0; i < num_targets; i++) {
        if (targets[i].state == INT_STATE_ACTIVE) return INT_STATE_ACTIVE;
    }
    return INT_STATE_IDLE;
}

bool is_captured() {
    for (int i = 0; i < num_targets; i++) {
        if (targets[i].state == INT_STATE_ACTIVE) return true;
    }
    return false;
}

int get_active_target_count() {
    int count = 0;
    for (int i = 0; i < num_targets; i++) {
        if (targets[i].state == INT_STATE_ACTIVE) count++;
    }
    return count;
}
