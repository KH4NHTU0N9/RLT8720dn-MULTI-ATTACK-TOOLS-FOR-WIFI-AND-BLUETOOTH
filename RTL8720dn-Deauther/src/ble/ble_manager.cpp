#include "ble_manager.h"
#include "BLEDevice.h"
#include "BLEScan.h"
#include "BLEClient.h"
#include "BLEConnect.h"
#include "../../debug.h"
#include "../config/app_config.h"
#include "../core/led_controller.h"
#include "../oled/oled_display.h"
#include "../security/security_utils.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "gap_le_types.h"
#include "gap_callback_le.h"
#ifdef __cplusplus
}
#endif

std::vector<BLEAuditResult> g_ble_results;
static volatile bool g_ble_scan_accepting = false;

static const uint8_t kAppleModels[][2] = {
    {0x02, 0x20}, // AirPods
    {0x0E, 0x20}, // AirPods Pro
    {0x0A, 0x20}, // AirPods Max
    {0x0F, 0x20}, // AirPods 2nd Gen
    {0x13, 0x20}, // AirPods 3rd Gen
    {0x14, 0x20}, // AirPods Pro 2nd Gen
    {0x03, 0x20}, // PowerBeats3
    {0x0B, 0x20}, // PowerBeats Pro
    {0x0C, 0x20}, // Beats Solo Pro
    {0x11, 0x20}, // Beats Studio Buds
    {0x10, 0x20}, // Beats Flex
    {0x05, 0x20}, // AppleTV Setup
    {0x06, 0x20}, // HomePod Setup
    {0x09, 0x20}, // AppleTV Connect
};
