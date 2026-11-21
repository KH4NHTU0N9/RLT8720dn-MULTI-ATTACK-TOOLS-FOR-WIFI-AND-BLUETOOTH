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
#endif // APP_STATE_H
