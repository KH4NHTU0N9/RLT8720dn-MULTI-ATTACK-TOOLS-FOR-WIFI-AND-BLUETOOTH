#undef min
#undef max
#include <Arduino.h>

#include "WiFi.h"
#include "WiFiServer.h"
#include "wifi_conf.h"
#include "debug.h"
#include "src/config/app_config.h"
#include "src/core/app_state.h"
#include "src/core/attack_runner.h"
#include "src/core/led_controller.h"
#include "src/oled/oled_display.h"
#include "src/wifi/wifi_scanner.h"
#include "src/ble/ble_manager.h"
#include "src/web/web_router.h"

AppState g_app;
WiFiServer server(80);

void setup() {
  ledInit();
  DEBUG_SER_INIT();
  DEBUG_SER_PRINT("\n[*] Booting RTL8720dn-Deauther...\n");

  oled_init();
  oled_show_boot();
  delay(BOOT_SPLASH_MS);

  wifiScannerInit();
  bleManagerInit();

  wifiRestoreSoftAp(g_app.config.current_channel);

  oled_show_scanning();
  wifiScanNetworks();

#ifdef DEBUG
  for (size_t i = 0; i < g_scan_results.size(); i++) {
    DEBUG_SER_PRINT(g_scan_results[i].ssid);
    DEBUG_SER_PRINT(" ");
    debugPrintMac(g_scan_results[i].bssid);
    DEBUG_SER_PRINT(" CH:");
    DEBUG_SER_PRINT((int)g_scan_results[i].channel);
    DEBUG_SER_PRINT(" RSSI:");
    DEBUG_SER_PRINTLN((int)g_scan_results[i].rssi);
  }
#endif

