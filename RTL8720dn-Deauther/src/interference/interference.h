#ifndef INTERFERENCE_H
#define INTERFERENCE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include "../config/app_config.h"
#include "../packet-injection/packet-injection.h"

typedef enum {
    INT_STATE_IDLE,
    INT_STATE_CAPTURING,
    INT_STATE_ACTIVE
} interference_state_t;

typedef struct {
    uint8_t bssid[6];
    uint8_t* payload;
    size_t payload_len;
    int target_channel;
    uint32_t start_time;
    interference_state_t state;
    uint16_t beacon_interval;
    uint16_t capabilities;
    uint32_t cycle_count;
} BeaconClone;

void start_interference(uint8_t* target_bssid, int channel);
void start_interference_multi();
bool add_interference_target(uint8_t* target_bssid, int channel);
void stop_interference();
uint32_t run_interference_cycle();
interference_state_t get_interference_state();
bool is_captured();
int get_active_target_count();

// Exposed for host-side unit testing of sniffer validation
size_t interference_sanitize_beacon_tags(const uint8_t* tags, size_t raw_len, size_t max_len);

#endif // INTERFERENCE_H
