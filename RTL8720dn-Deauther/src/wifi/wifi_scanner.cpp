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

