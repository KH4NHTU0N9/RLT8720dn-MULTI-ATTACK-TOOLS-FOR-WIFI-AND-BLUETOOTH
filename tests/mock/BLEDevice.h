#ifndef MOCK_BLE_DEVICE_H
#define MOCK_BLE_DEVICE_H

#include "BLEScan.h"
#include "BLEClient.h"
#include "BLEConnect.h"
#include <cstdint>
#include <vector>

class BLEAdvert {
public:
    uint8_t last_adv_type = 0;
    uint16_t min_interval = 0;
    uint16_t max_interval = 0;
    std::vector<uint8_t> last_adv_data;
    int start_calls = 0;
    int stop_calls = 0;

#endif // MOCK_BLE_DEVICE_H
