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
