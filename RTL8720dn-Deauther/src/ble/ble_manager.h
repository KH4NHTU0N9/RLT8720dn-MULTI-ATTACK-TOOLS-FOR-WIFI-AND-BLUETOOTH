#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "../core/app_state.h"

#define BLE_APPLE_PAYLOAD_LEN   30
#define BLE_GOOGLE_PAYLOAD_LEN  14
#define BLE_WINDOWS_PAYLOAD_LEN 18

enum class BleWriteStatus : uint8_t {
    BLE_WRITE_OK = 0,
    BLE_WRITE_CONNECT_FAILED,
    BLE_WRITE_CLIENT_FAILED,
    BLE_WRITE_DISCOVERY_TIMEOUT,
    BLE_WRITE_SERVICE_NOT_FOUND,
    BLE_WRITE_CHAR_NOT_FOUND,
    BLE_WRITE_FAILED
};

#endif // BLE_MANAGER_H
