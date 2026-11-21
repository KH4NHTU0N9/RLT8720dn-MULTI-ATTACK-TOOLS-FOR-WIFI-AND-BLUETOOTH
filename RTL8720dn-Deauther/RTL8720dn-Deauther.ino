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

