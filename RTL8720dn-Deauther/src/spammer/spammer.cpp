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

}
