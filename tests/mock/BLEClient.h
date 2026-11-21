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

    BLERemoteCharacteristic* getCharacteristic(const char* target_uuid) {
        if (!characteristic || !target_uuid) return nullptr;
        return (characteristic->uuid == target_uuid) ? characteristic : nullptr;
    }
};

class BLEClient {
public:
    bool is_connected = true;
    bool disc_done = true;
    int discover_calls = 0;
    BLERemoteService* service = nullptr;

    void discoverServices() { discover_calls++; }
    bool discoveryDone() const { return disc_done; }
    bool connected() const { return is_connected; }

    BLERemoteService* getService(const char* target_uuid) {
        if (!service || !target_uuid) return nullptr;
        return (service->uuid == target_uuid) ? service : nullptr;
    }
};

#endif // MOCK_BLE_CLIENT_H
