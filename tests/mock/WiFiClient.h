#ifndef MOCK_WIFI_CLIENT_H
#define MOCK_WIFI_CLIENT_H

#include "Arduino.h"
#include <cstddef>
#include <cstdint>

class WiFiClient {
public:
    bool connected() { return false; }
    int available() { return 0; }
    int read() { return -1; }
    size_t write(const uint8_t* buf, size_t size) {
        (void)buf;
        return size;
    }
    void stop() {}
};

#endif // MOCK_WIFI_CLIENT_H
