#ifndef MOCK_WIFI_CONF_H
#define MOCK_WIFI_CONF_H

#include "Arduino.h"
#include "wifi_structures.h"
#include <vector>

#define WLAN0_NAME "wlan0"

struct MockWifiState {
    int current_mode = RTW_MODE_AP;
    int last_channel = 1;
    std::vector<int> channel_history;
    bool powersave_disabled = false;
    bool wifi_scan_should_fail = false;

#endif // MOCK_WIFI_CONF_H
