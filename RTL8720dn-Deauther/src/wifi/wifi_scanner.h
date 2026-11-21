#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <vector>
#include "../core/app_state.h"

extern std::vector<WiFiScanResult> g_scan_results;

#endif // WIFI_SCANNER_H
