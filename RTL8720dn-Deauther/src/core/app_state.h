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

#endif // APP_STATE_H
