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
        in_promisc_callback = false;
        self_deadlock_detected = false;
        active_scan_handler = nullptr;
        canned_scan_results.clear();
    }
};

inline MockWifiState& mockWifi() {
    static MockWifiState st;
    return st;
}

static inline int wext_set_channel(const char* ifname, int ch) {
    (void)ifname;
    mockWifi().last_channel = ch;
    mockWifi().channel_history.push_back(ch);
    return 0;
}

extern "C" {
static inline void wifi_enter_promisc_mode() {
    mockWifi().enter_promisc_calls++;
}

static inline void wifi_set_promisc(rtw_rcr_level_t enabled, promisc_callback_t callback, unsigned char len_used) {
    MockWifiState& w = mockWifi();
    w.set_promisc_calls++;
    if (w.in_promisc_callback) {
        w.self_deadlock_detected = true;
    }
    w.promisc_level = enabled;
    w.promisc_cb = callback;
    w.promisc_len_used = len_used;
}

static inline int wifi_disable_powersave() {
    mockWifi().powersave_disabled = true;
    return 0;
}

static inline int wifi_on(int mode) {
    mockWifi().current_mode = mode;
    mockWifi().powersave_disabled = false;
    return 0;
}

static inline int wifi_off() {
    mockWifi().current_mode = RTW_MODE_NONE;
    return 0;
}

static inline rtw_result_t wifi_scan_networks(scan_result_handler_t results_handler, void* user_data) {
    (void)user_data;
    MockWifiState& w = mockWifi();
    if (w.wifi_scan_should_fail || !results_handler) {
        return RTW_ERROR;
    }
    w.active_scan_handler = results_handler;
    for (size_t i = 0; i < w.canned_scan_results.size(); i++) {
        rtw_scan_handler_result_t item{};
        item.scan_complete = 0;
        item.ap_details = w.canned_scan_results[i];
        results_handler(&item);
    }
    rtw_scan_handler_result_t done{};
    done.scan_complete = 1;
    results_handler(&done);
    return RTW_SUCCESS;
}
} // extern "C"

static inline void mockWifiInjectPromiscFrame(unsigned char* frame, unsigned int len, void* arg = nullptr) {
    MockWifiState& w = mockWifi();
    if (w.promisc_cb && w.promisc_level != RTW_PROMISC_DISABLE) {
        w.in_promisc_callback = true;
        w.promisc_cb(frame, w.promisc_len_used ? len : 0, arg);
        w.in_promisc_callback = false;
    }
}

#endif // MOCK_WIFI_CONF_H
