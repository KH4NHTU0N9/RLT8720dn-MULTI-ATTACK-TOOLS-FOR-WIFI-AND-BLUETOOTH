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
