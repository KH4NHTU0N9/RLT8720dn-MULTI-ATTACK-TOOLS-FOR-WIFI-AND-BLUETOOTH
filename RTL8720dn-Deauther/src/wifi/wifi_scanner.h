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

// Initialize scan storage (pre-reserves capacity to avoid heap fragmentation).
void wifiScannerInit();

// Execute a bounded Wi-Fi scan (2.4 GHz + 5 GHz) waiting cleanly for callback completion.
// Returns 0 on success, 1 on failure.
int wifiScanNetworks();

// Count scanned networks by band (2.4 GHz vs 5 GHz).
void wifiScanCountByBand(int* count_24, int* count_5g);

// Restore SoftAP mode on target channel and disable power saving.
void wifiRestoreSoftAp(int channel);

#endif // WIFI_SCANNER_H
