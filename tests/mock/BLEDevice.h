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

    void setAdvType(uint8_t t) { last_adv_type = t; }
    void setMinInterval(uint16_t v) { min_interval = v; }
    void setMaxInterval(uint16_t v) { max_interval = v; }
    void setAdvData(uint8_t* data, uint8_t len) {
        if (data && len > 0) last_adv_data.assign(data, data + len);
        else last_adv_data.clear();
    }
    void stopAdv() { stop_calls++; }
    void updateAdvertParams() {}
    void startAdv() { start_calls++; }
};

class MockBLEDevice {
public:
    BLEScan scan;
    BLEConnect conn;
    BLEClient client;
    BLEAdvert advert;

    bool client_configured = false;
    bool add_client_without_config = false;
    bool conn_is_connected = true;
    bool return_null_client = false;
    int end_calls = 0;
    int init_calls = 0;

#endif // MOCK_BLE_DEVICE_H
