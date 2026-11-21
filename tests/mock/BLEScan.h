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

    void startScan(uint32_t ms) {
        last_scan_ms = ms;
        if (scan_cb) {
            for (size_t i = 0; i < canned_devices.size(); i++) {
                T_LE_CB_DATA cb_data{};
                cb_data.p_le_scan_info = &canned_devices[i];
                scan_cb(&cb_data);
            }
        }
        delay(ms);
    }
};

#endif // MOCK_BLE_SCAN_H
