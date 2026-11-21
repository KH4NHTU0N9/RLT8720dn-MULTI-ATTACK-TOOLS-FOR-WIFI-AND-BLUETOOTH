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

    int begin_calls = 0;
    int stop_calls = 0;

private:
    int _port;
    bool _running;
};

#endif // MOCK_WIFI_SERVER_H
