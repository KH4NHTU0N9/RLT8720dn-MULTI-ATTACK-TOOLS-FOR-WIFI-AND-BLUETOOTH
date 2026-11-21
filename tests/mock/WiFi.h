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

#endif // MOCK_WIFI_H
