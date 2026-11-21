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
