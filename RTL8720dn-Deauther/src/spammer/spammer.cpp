#include "spammer.h"
#include "wifi_conf.h"
#include "../../debug.h"
#include <string.h>

SSIDSpammer Spammer;

SSIDSpammer::SSIDSpammer()
    : _active(false), _randomMode(false), _randomCount(10), _currentChannel(1), _lastChannelHop(0) {
}

SSIDSpammer::~SSIDSpammer() {
    _active = false;
}

void SSIDSpammer::addSSID(const String& ssid) {
    if (_networks.size() >= MAX_SPAM_NETWORKS) return;
    if (ssid.length() == 0) return;

    SpammedNetwork net;
    net.ssid = (ssid.length() > MAX_SSID_LEN) ? ssid.substring(0, MAX_SSID_LEN) : ssid;
    _generateRandomMAC(net.bssid);
    _networks.push_back(net);
}

void SSIDSpammer::clearSSIDs() {
    _networks.clear();
}

void SSIDSpammer::setRandomMode(bool enable, int count) {
    _randomMode = enable;
    if (count < 1) count = 1;
    if (count > MAX_SPAM_NETWORKS) count = MAX_SPAM_NETWORKS;
    _randomCount = count;
}

void SSIDSpammer::start() {
    if (_active) return;

    if (_randomMode) {
        clearSSIDs();
        _networks.reserve((size_t)_randomCount);
        for (int i = 0; i < _randomCount; i++) {
            addSSID(_generateRandomSSID(8 + (int)random(8)));
        }
    }

    if (_networks.empty()) {
        DEBUG_SER_PRINT("[!] Spammer: No SSIDs to spam.\n");
        return;
    }

    _active = true;
    _currentChannel = 1;
    _lastChannelHop = millis();
    wext_set_channel(WLAN0_NAME, _currentChannel);
    DEBUG_SER_PRINT("[+] Spammer: Started flooding with ");
    DEBUG_SER_PRINT((unsigned long)_networks.size());
    DEBUG_SER_PRINT(" networks.\n");
}

void SSIDSpammer::stop() {
    if (!_active) return;
    _active = false;
    DEBUG_SER_PRINT("[-] Spammer: Stopped.\n");
}

void SSIDSpammer::_hopChannelIfDue() {
    uint32_t now = millis();
    if ((uint32_t)(now - _lastChannelHop) >= 100) {
        _currentChannel++;
        if (_currentChannel > 13) _currentChannel = 1;
        wext_set_channel(WLAN0_NAME, _currentChannel);
        _lastChannelHop = now;
    }
}

uint32_t SSIDSpammer::run() {
    if (!_active || _networks.empty()) return 0;

    uint32_t sent = 0;
    for (size_t i = 0; i < _networks.size(); i++) {
        _hopChannelIfDue();
        if (_craftAndSend(_networks[i].ssid, _networks[i].bssid, _currentChannel)) {
            sent++;
        }
        // Yield every 8 frames to balance throughput and FreeRTOS WDT feeding
        if ((i & 7U) == 7U) {
            delay(1);
        }
    }
    delay(1);
    return sent;
}

void SSIDSpammer::_generateRandomMAC(uint8_t* mac) {
    if (!mac) return;
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)random(256);
    }
    mac[0] &= 0xFE; // Unicast
    mac[0] |= 0x02; // Locally Administered Address
}

String SSIDSpammer::_generateRandomSSID(int length) {
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (length < 1) length = 1;
    if (length > MAX_SSID_LEN) length = MAX_SSID_LEN;

}
