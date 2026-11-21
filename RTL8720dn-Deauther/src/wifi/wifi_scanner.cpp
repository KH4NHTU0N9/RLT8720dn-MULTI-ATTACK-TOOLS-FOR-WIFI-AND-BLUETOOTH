#include "wifi_scanner.h"
#include "WiFi.h"
#include "wifi_conf.h"
#include "wifi_structures.h"
#include "../../debug.h"
#include "../config/app_config.h"
#include "../security/security_utils.h"
#include <string.h>

std::vector<WiFiScanResult> g_scan_results;
static volatile bool g_scan_done = false;
static volatile bool g_scan_accepting = false;

static rtw_result_t _scanResultHandler(rtw_scan_handler_result_t* scan_result) {
    if (scan_result == nullptr) return RTW_SUCCESS;

    if (scan_result->scan_complete != 0) {
        g_scan_accepting = false;
        g_scan_done = true;
        return RTW_SUCCESS;
    }

    if (!g_scan_accepting || g_scan_results.size() >= MAX_SCAN_RESULTS) {
        return RTW_SUCCESS;
    }

    rtw_scan_result_t* record = &scan_result->ap_details;
    WiFiScanResult result;
    result.ssid = sanitizeSsid(record->SSID.val, record->SSID.len);
    result.channel = (uint8_t)record->channel;
    result.rssi = (int16_t)record->signal_strength;
    memcpy(result.bssid, &record->BSSID, 6);
    g_scan_results.push_back(result);
    return RTW_SUCCESS;
}

void wifiScannerInit() {
    g_scan_results.reserve(MAX_SCAN_RESULTS);
}

int wifiScanNetworks() {
    DEBUG_SER_PRINT("Scanning WiFi networks...");
    g_scan_accepting = false;
    g_scan_done = false;
    g_scan_results.clear();
    g_scan_accepting = true;

    if (wifi_scan_networks(_scanResultHandler, NULL) != RTW_SUCCESS) {
        g_scan_accepting = false;
        DEBUG_SER_PRINT(" failed!\n");
        return 1;
    }

    const unsigned long start_ms = millis();
    while (!g_scan_done && (unsigned long)(millis() - start_ms) < 12500UL) {
        delay(50);
    }
    g_scan_accepting = false;
    g_app.channel_needs_restore = true;

    DEBUG_SER_PRINT(" done! (");
    DEBUG_SER_PRINT((unsigned long)g_scan_results.size());
    DEBUG_SER_PRINTLN(" APs)");
    return 0;
}

void wifiScanCountByBand(int* count_24, int* count_5g) {
    int c24 = 0;
    int c5g = 0;
    for (size_t i = 0; i < g_scan_results.size(); i++) {
        if (is5GhzChannel(g_scan_results[i].channel)) {
            c5g++;
        } else {
            c24++;
        }
    }
    if (count_24) *count_24 = c24;
    if (count_5g) *count_5g = c5g;
}

void wifiRestoreSoftAp(int channel) {
    char ch_buf[8];
    snprintf(ch_buf, sizeof(ch_buf), "%d", channel);
    WiFi.apbegin((char*)DEFAULT_AP_SSID, (char*)DEFAULT_AP_PASS, ch_buf);
    wifi_disable_powersave();
    g_app.channel_needs_restore = true;
}
