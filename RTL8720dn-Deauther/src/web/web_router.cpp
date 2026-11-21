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

