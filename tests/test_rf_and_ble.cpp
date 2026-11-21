#include <Arduino.h>
#include "wifi_conf.h"
#include "BLEDevice.h"
#undef min
#undef max
#include "ble/ble_manager.h"
#include "config/app_config.h"
#include "core/app_state.h"
#include "core/attack_runner.h"
#include "core/led_controller.h"
#include "interference/interference.h"
#include "oled/oled_display.h"
#include "packet-injection/packet-injection.h"
#include "spammer/spammer.h"
#include "wifi/wifi_scanner.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond)                                                          \
    do {                                                                           \
        g_tests_run++;                                                             \
        if (!(cond)) {                                                             \
            g_tests_failed++;                                                      \
            std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                          \
    } while (0)

