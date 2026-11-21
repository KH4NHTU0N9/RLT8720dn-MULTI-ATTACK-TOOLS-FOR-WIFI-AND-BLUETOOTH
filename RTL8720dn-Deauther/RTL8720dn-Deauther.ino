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

  server.begin();

  int c24 = 0, c5g = 0;
  wifiScanCountByBand(&c24, &c5g);
  oled_show_idle(c24, c5g);
  ledShowIdle();
}

void loop() {
  if (g_app.attack_mode != AttackMode::IDLE) {
    attackRunTick(server);
  }

  webServeClient(server);

  static int last_idle_channel = -1;
  if (g_app.channel_needs_restore || last_idle_channel != g_app.config.current_channel) {
    wext_set_channel(WLAN0_NAME, g_app.config.current_channel);
    last_idle_channel = g_app.config.current_channel;
    g_app.channel_needs_restore = false;
  }
  delay(IDLE_LOOP_DELAY_MS);
}
