#ifndef MOCK_WIFI_SERVER_H
#define MOCK_WIFI_SERVER_H

#include "WiFiClient.h"

class WiFiServer {
public:
    explicit WiFiServer(int port = 80) : _port(port), _running(false) {}
    void begin() { _running = true; begin_calls++; }
    void stop() { _running = false; stop_calls++; }
    WiFiClient available() { return WiFiClient(); }
    bool isRunning() const { return _running; }
    int port() const { return _port; }

#endif // MOCK_WIFI_SERVER_H
