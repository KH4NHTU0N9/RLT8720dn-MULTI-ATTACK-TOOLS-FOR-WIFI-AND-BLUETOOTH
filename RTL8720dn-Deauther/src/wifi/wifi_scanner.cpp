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
