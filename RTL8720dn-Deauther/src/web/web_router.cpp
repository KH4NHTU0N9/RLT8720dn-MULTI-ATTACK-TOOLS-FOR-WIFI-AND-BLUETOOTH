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

static bool _isGetOrHeadMethod(const char* method) {
    return method && (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0);
}

static void _extractPostParams(const char* raw_req, size_t req_len,
                               std::vector<std::pair<String, String>>& params) {
    size_t hdr_end = _findHeaderEnd(raw_req, req_len);
    if (hdr_end == (size_t)-1 || hdr_end + 4 >= req_len) {
        params.clear();
        return;
    }
    webParsePostBody(raw_req + hdr_end + 4, req_len - (hdr_end + 4), params);
}

static void _handleWifiAttackPost(const std::vector<std::pair<String, String>>& params,
                                  bool is_interference,
                                  IWebOutput& output,
                                  void (*prepare_attack_cb)(void*), void* cb_ctx) {
    g_deauth_channels.clear();
    std::vector<int> targets;
    targets.reserve(MAX_SCAN_RESULTS);

    for (size_t i = 0; i < params.size(); i++) {
        const String& k = params[i].first;
        const String& v = params[i].second;
        if (k == "target" || k == "network" || k == "interfere") {
            long idx = -1;
            if (parseStrictIntRange(v, 0, (long)g_scan_results.size() - 1, &idx)) {
                targets.push_back((int)idx);
            }
        } else if (k == "frames") {
            g_app.config.frames_per_deauth = webClampFramesPerDeauth(v, g_app.config.frames_per_deauth);
        } else if (k == "reason") {
            g_app.config.deauth_reason = webClampDeauthReason(v, g_app.config.deauth_reason);
        }
    }

    if (targets.empty()) {
        g_app.notice.set(StatusKind::STATUS_WARN, "Select at least 1 Wi-Fi target before starting.");
        _writeRedirect(output, "/");
        return;
    }

    if (is_interference) {
        g_app.attack_mode = AttackMode::INTERFERENCE;
        g_app.sent_frames = 0;
        _writeRedirect(output, "/");
        if (prepare_attack_cb) prepare_attack_cb(cb_ctx);

        if (targets.size() == 1) {
            int idx = targets[0];
            start_interference(g_scan_results[(size_t)idx].bssid, g_scan_results[(size_t)idx].channel);
        } else {
            start_interference_multi();
            for (size_t i = 0; i < targets.size() && i < MAX_INTERFERENCE_TARGETS; i++) {
                int idx = targets[i];
                add_interference_target(g_scan_results[(size_t)idx].bssid, g_scan_results[(size_t)idx].channel);
            }
        }
        return;
    }

    for (size_t i = 0; i < targets.size(); i++) {
        int idx = targets[i];
        int ch = g_scan_results[(size_t)idx].channel;
        g_deauth_channels[ch].push_back(idx);
    }

    g_app.attack_mode = AttackMode::DEAUTH;
    g_app.sent_frames = 0;
    _writeRedirect(output, "/");
    if (prepare_attack_cb) prepare_attack_cb(cb_ctx);
}

static void _handleSpamPost(const std::vector<std::pair<String, String>>& params,
                            IWebOutput& output,
                            void (*prepare_attack_cb)(void*), void* cb_ctx) {
    std::vector<String> ssids;
    int random_count = 0;

    for (size_t i = 0; i < params.size(); i++) {
        const String& k = params[i].first;
        const String& v = params[i].second;
        if (k == "count") {
            g_app.config.beacons_per_ssid = webClampBeaconsPerSsid(v, g_app.config.beacons_per_ssid);
        } else if (k == "random_count") {
            random_count = webClampRandomSsidCount(v);
        } else if (k == "ssids") {
            const char* p = v.c_str();
            size_t len = v.length();
            size_t start = 0;
            while (start <= len && ssids.size() < MAX_SPAM_NETWORKS) {
                size_t end = start;
                while (end < len && p[end] != '\n') end++;
                String line;
                line.reserve((unsigned int)(end - start));
                for (size_t j = start; j < end; j++) {
                    if (p[j] != '\r') line += p[j];
                }
                line.trim();
                if (line.length() > 0) {
                    ssids.push_back(line);
                }
                if (end >= len) break;
                start = end + 1;
            }
        }
    }

    if (random_count <= 0 && ssids.empty()) {
        g_app.notice.set(StatusKind::STATUS_WARN, "Enter at least 1 SSID or set Random SSIDs > 0.");
        _writeRedirect(output, "/");
        return;
    }

    Spammer.clearSSIDs();
    if (random_count > 0) {
        Spammer.setRandomMode(true, random_count);
    } else {
        Spammer.setRandomMode(false);
        for (size_t i = 0; i < ssids.size(); i++) {
            for (int c = 0; c < g_app.config.beacons_per_ssid; c++) {
                Spammer.addSSID(ssids[i]);
            }
        }
    }

    g_app.attack_mode = AttackMode::BEACON_SPAM;
    g_app.sent_frames = 0;
    _writeRedirect(output, "/");
    if (prepare_attack_cb) prepare_attack_cb(cb_ctx);
    Spammer.start();
}

static void _handleBleWritePost(const std::vector<std::pair<String, String>>& params, IWebOutput& output) {
    String mac, service_uuid, char_uuid, hex_data;
    long addr_type = 0;

    for (size_t i = 0; i < params.size(); i++) {
        const String& k = params[i].first;
        const String& v = params[i].second;
        if (k == "mac") mac = v;
        else if (k == "addr_type") parseStrictIntRange(v, 0, 1, &addr_type);
        else if (k == "service") service_uuid = v;
        else if (k == "char") char_uuid = v;
        else if (k == "data") hex_data = v;
    }

