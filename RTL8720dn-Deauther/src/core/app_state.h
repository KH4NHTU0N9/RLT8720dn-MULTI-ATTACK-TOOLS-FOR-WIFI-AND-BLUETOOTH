#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#undef min
#undef max
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstring>
#include "../config/app_config.h"

enum class AttackMode : uint8_t {
    IDLE = 0,
    DEAUTH,
    INTERFERENCE,
    BEACON_SPAM,
    BLE_SPAM
};

enum class StatusKind : uint8_t {
    STATUS_NONE = 0,
    STATUS_INFO,
    STATUS_OK,
    STATUS_WARN,
    STATUS_ERR
};

struct StatusNotice {
    StatusKind kind = StatusKind::STATUS_NONE;
    char message[96] = {0};

    void clear() {
        kind = StatusKind::STATUS_NONE;
        message[0] = '\0';
    }

    void set(StatusKind k, const char* msg) {
        kind = k;
        if (!msg) {
            message[0] = '\0';
            return;
        }
        std::snprintf(message, sizeof(message), "%s", msg);
    }
};

struct AppConfig {
    int frames_per_deauth = DEFAULT_FRAMES_PER_DEAUTH;
    int beacons_per_ssid = DEFAULT_BEACONS_PER_SSID;
    uint16_t deauth_reason = DEFAULT_DEAUTH_REASON;
    int ble_spam_type = 0;
    int current_channel = DEFAULT_AP_CHANNEL;
};

struct WiFiScanResult {
    String ssid;
    uint8_t bssid[6];
    int16_t rssi;
    uint8_t channel;
};

struct BLEAuditResult {
    String name;
    char address[18];
    uint8_t addr_type;
    int16_t rssi;
};

struct AppState {
    AttackMode attack_mode = AttackMode::IDLE;
    bool channel_needs_restore = true;
    uint32_t sent_frames = 0;
    AppConfig config;
    StatusNotice notice;
};

extern AppState g_app;

#endif // APP_STATE_H
