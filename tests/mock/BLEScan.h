#ifndef MOCK_BLE_SCAN_H
#define MOCK_BLE_SCAN_H

#include "Arduino.h"
#include "gap_callback_le.h"
#include <cstdint>
#include <vector>

class BLEScan {
public:
    std::vector<T_LE_SCAN_INFO> canned_devices;
    void (*scan_cb)(T_LE_CB_DATA*) = nullptr;
    uint32_t last_scan_ms = 0;

#endif // MOCK_BLE_SCAN_H
