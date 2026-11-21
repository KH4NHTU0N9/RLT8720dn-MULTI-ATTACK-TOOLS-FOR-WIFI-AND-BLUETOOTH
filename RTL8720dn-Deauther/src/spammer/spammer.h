#ifndef SPAMMER_H
#define SPAMMER_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "../config/app_config.h"
#include "../packet-injection/packet-injection.h"

#define BEACON_FIXED_LEN 36
#define MAX_SSID_LEN     MAX_SSID_BYTES

    std::vector<SpammedNetwork> _networks;
    bool _active;
    bool _randomMode;
    int _randomCount;
    int _currentChannel;
    uint32_t _lastChannelHop;
};

extern SSIDSpammer Spammer;

#endif // SPAMMER_H
