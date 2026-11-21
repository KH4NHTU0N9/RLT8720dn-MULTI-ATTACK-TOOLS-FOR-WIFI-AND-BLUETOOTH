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
