#ifndef MOCK_BLE_CLIENT_H
#define MOCK_BLE_CLIENT_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class BLERemoteCharacteristic {
public:
    std::string uuid;
    std::vector<uint8_t> written_bytes;
    bool set_data_result = true;

    bool setData(uint8_t* data, uint16_t size) {
        if (!set_data_result || !data || size == 0) return false;
        written_bytes.assign(data, data + size);
        return true;
    }
};

class BLERemoteService {
public:
    std::string uuid;
    BLERemoteCharacteristic* characteristic = nullptr;

#endif // MOCK_BLE_CLIENT_H
