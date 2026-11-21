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

#endif // MOCK_BLE_CLIENT_H
