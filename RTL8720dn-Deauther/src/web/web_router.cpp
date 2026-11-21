#include "web_router.h"
#include "../../debug.h"
#include "../ble/ble_manager.h"
#include "../config/app_config.h"
#include "../core/attack_runner.h"
#include "../core/led_controller.h"
#include "../interference/interference.h"
#include "../oled/oled_display.h"
#include "../security/security_utils.h"
#include "../spammer/spammer.h"
#include "../wifi/wifi_scanner.h"
#include "web_ui.h"
#include <string.h>

#ifndef UNIT_TEST
#include "WiFiClient.h"
#include "WiFiServer.h"
extern "C" size_t xPortGetFreeHeapSize(void);

class WiFiClientOutput : public IWebOutput {
public:
    explicit WiFiClientOutput(WiFiClient& c) : _c(c) {}
    size_t writeBytes(const uint8_t* data, size_t len) override {
        return _c.write(data, len);
    }
private:
    WiFiClient& _c;
};
#endif

static void _writeRedirect(IWebOutput& output, const char* url) {
    String hdr = buildHttpRedirectHeaders(url ? url : "/");
    output.writeBytes((const uint8_t*)hdr.c_str(), hdr.length());
}

void webParsePostBody(const char* body, size_t body_len,
                      std::vector<std::pair<String, String>>& out_params) {
    out_params.clear();
    if (!body || body_len == 0) return;

    // Estimate pair count to minimize vector reallocations
    size_t est = 1;
    for (size_t i = 0; i < body_len; i++) {
        if (body[i] == '&') est++;
    }
    out_params.reserve(est);

    size_t start = 0;
    while (start <= body_len) {
        size_t end = start;
        while (end < body_len && body[end] != '&') end++;

        const size_t pair_len = end - start;
        if (pair_len > 0) {
            size_t eq = start;
            while (eq < end && body[eq] != '=') eq++;
            if (eq < end) {
                String raw_key;
                raw_key.reserve((unsigned int)(eq - start));
                for (size_t k = start; k < eq; k++) raw_key += body[k];

                String raw_val;
                raw_val.reserve((unsigned int)(end - (eq + 1)));
                for (size_t v = eq + 1; v < end; v++) raw_val += body[v];

                out_params.push_back(std::make_pair(urlDecode(raw_key), urlDecode(raw_val)));
            }
        }
        if (end >= body_len) break;
        start = end + 1;
    }
}

int webClampFramesPerDeauth(const String& raw, int fallback) {
    long val = 0;
    if (!parseStrictIntRange(raw, MIN_FRAMES_PER_DEAUTH, MAX_FRAMES_PER_DEAUTH, &val)) {
        return fallback;
    }
    return (int)val;
}

uint16_t webClampDeauthReason(const String& raw, uint16_t fallback) {
    long val = 0;
    if (!parseStrictIntRange(raw, 1, 65535, &val)) {
        return fallback;
    }
    return (uint16_t)val;
}

int webClampBeaconsPerSsid(const String& raw, int fallback) {
    long val = 0;
    if (!parseStrictIntRange(raw, 1, MAX_BEACONS_PER_SSID, &val)) {
        return fallback;
    }
    return (int)val;
}

int webClampRandomSsidCount(const String& raw) {
    long val = 0;
    if (!parseStrictIntRange(raw, 0, MAX_SPAM_NETWORKS, &val)) {
        return 0;
    }
    return (int)val;
}

int webClampBleSpamType(const String& raw) {
    long val = 0;
    if (!parseStrictIntRange(raw, 0, 2, &val)) {
        return 0;
    }
    return (int)val;
}

static size_t _findHeaderEnd(const char* buf, size_t len) {
    if (!buf || len < 4) return (size_t)-1;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return i;
        }
    }
    return (size_t)-1;
}

static bool _isPostMethod(const char* method) {
    return method && strcmp(method, "POST") == 0;
}
