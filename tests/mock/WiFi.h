#ifndef MOCK_WIFI_H
#define MOCK_WIFI_H

#include "Arduino.h"
#include "wifi_conf.h"
#include <string>

class MockWiFiClass {
public:
    std::string ap_ssid;
    std::string ap_pass;
    std::string ap_channel;
    int apbegin_calls = 0;

    int apbegin(char* ssid, char* pass, char* channel) {
        ap_ssid = ssid ? ssid : "";
        ap_pass = pass ? pass : "";
        ap_channel = channel ? channel : "";
        apbegin_calls++;
        mockWifi().current_mode = RTW_MODE_AP;
        return 1;
    }
};

inline MockWiFiClass& mockWiFiClassInstance() {
    static MockWiFiClass w;
    return w;
}

#define WiFi mockWiFiClassInstance()

#endif // MOCK_WIFI_H
