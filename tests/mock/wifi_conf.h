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

    rtw_rcr_level_t promisc_level = RTW_PROMISC_DISABLE;
    promisc_callback_t promisc_cb = nullptr;
    unsigned char promisc_len_used = 0;
    int enter_promisc_calls = 0;
    int set_promisc_calls = 0;
    bool in_promisc_callback = false;
    bool self_deadlock_detected = false;

    scan_result_handler_t active_scan_handler = nullptr;
    std::vector<rtw_scan_result_t> canned_scan_results;

    void reset() {
        current_mode = RTW_MODE_AP;
        last_channel = 1;
        channel_history.clear();
        powersave_disabled = false;
        wifi_scan_should_fail = false;
        promisc_level = RTW_PROMISC_DISABLE;
        promisc_cb = nullptr;
        promisc_len_used = 0;
        enter_promisc_calls = 0;
        set_promisc_calls = 0;
#endif // MOCK_WIFI_CONF_H
