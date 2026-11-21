#include <Arduino.h>
#include "wifi_conf.h"
#undef min
#undef max
#include "ble/ble_manager.h"
#include "config/app_config.h"
#include "core/app_state.h"
#include "core/attack_runner.h"
#include "core/led_controller.h"
#include "interference/interference.h"
#include "packet-injection/packet-injection.h"
#include "spammer/spammer.h"
#include "web/web_router.h"
#include "web/web_stream.h"
#include "web/web_ui.h"
#include "wifi/wifi_scanner.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond)                                                          \
    do {                                                                           \
        g_tests_run++;                                                             \
        if (!(cond)) {                                                             \
            g_tests_failed++;                                                      \
            std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                          \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(actual_val, expected_val)                                    \
    do {                                                                           \
        g_tests_run++;                                                             \
        long long _a = static_cast<long long>(actual_val);                         \
        long long _e = static_cast<long long>(expected_val);                       \
        if (_a != _e) {                                                            \
            g_tests_failed++;                                                      \
            std::fprintf(stderr, "FAIL [%s:%d]: expected %lld, got %lld\n",        \
                         __FILE__, __LINE__, _e, _a);                              \
        }                                                                          \
    } while (0)

#define ASSERT_STREQ(actual_str, expected_cstr)                                      \
    do {                                                                             \
        g_tests_run++;                                                               \
        String _act = (actual_str);                                                  \
        const char* _exp = (expected_cstr);                                          \
        if (_act != _exp) {                                                          \
            g_tests_failed++;                                                        \
            std::fprintf(stderr, "FAIL [%s:%d]: expected \"%s\", got \"%s\"\n",      \
                         __FILE__, __LINE__, _exp ? _exp : "(null)", _act.c_str());  \
        }                                                                            \
    } while (0)

AppState g_app;
static uint8_t g_fake_wlan_storage[64] = {0};
uint8_t* rltk_wlan_info = g_fake_wlan_storage;

extern "C" {
unsigned char rltk_wlan_running(unsigned char idx) { (void)idx; return 1; }
void* alloc_mgtxmitframe(void* ptr) { (void)ptr; return nullptr; }
void update_mgntframe_attrib(void* ptr, void* fc) { (void)ptr; (void)fc; }
int dump_mgntframe(void* ptr, void* fc) { (void)ptr; (void)fc; return 0; }
bool unit_test_capture_raw_frame(const void* frame, size_t length) {
    (void)frame;
    (void)length;
    return true;
}
}

class MemoryWebOutput : public IWebOutput {
public:
    std::string body;
    std::vector<size_t> chunk_sizes;
    size_t max_chunk = 0;
    bool fail_next_write = false;

    size_t writeBytes(const uint8_t* data, size_t len) override {
        if (fail_next_write) return 0;
        if (!data || len == 0) return 0;
        body.append(reinterpret_cast<const char*>(data), len);
        chunk_sizes.push_back(len);
        if (len > max_chunk) max_chunk = len;
        return len;
    }

    bool contains(const char* sub) const {
        return sub && body.find(sub) != std::string::npos;
    }
};

struct CallbackRecorder {
    int scan_calls = 0;
    int prepare_attack_calls = 0;
};

static bool testPostScanCb(void* ctx) {
    CallbackRecorder* rec = static_cast<CallbackRecorder*>(ctx);
    rec->scan_calls++;
    return true;
}

static void testPrepareAttackCb(void* ctx) {
    CallbackRecorder* rec = static_cast<CallbackRecorder*>(ctx);
    rec->prepare_attack_calls++;
}

static void resetWebState() {
    mockArduino().reset();
    mockWifi().reset();
    g_app = AppState{};
    g_scan_results.clear();
    g_ble_results.clear();
    g_deauth_channels.clear();
    stop_interference();
    Spammer.stop();
    Spammer.clearSSIDs();
    ledInit();
    ledSetEnabled(true);
}

static void test_web_stream_chunking_and_abort() {
    MemoryWebOutput out;
    {
        WebStream ws(out);
        for (int i = 0; i < 300; i++) {
            ws.writeStr("0123456789");
        }
        ws.writeHtmlEscaped("<a href=\"x\" title='y'>&</a>");
        ws.flush();
        ASSERT_FALSE(ws.aborted());
        ASSERT_EQ_INT(ws.totalBytes(), 3000U + 61U);
    }
    // Every single TCP write chunk MUST stay <= WEB_STREAM_CHUNK_SIZE (1024B)
    ASSERT_TRUE(out.max_chunk <= WEB_STREAM_CHUNK_SIZE);
    ASSERT_TRUE(out.chunk_sizes.size() >= 3);
    ASSERT_TRUE(out.contains("&lt;a href=&quot;x&quot; title=&#39;y&#39;&gt;&amp;&lt;/a&gt;"));

    // Aborted stream stops forwarding subsequent writes
    MemoryWebOutput fail_out;
    fail_out.fail_next_write = true;
    WebStream ws_abort(fail_out);
    ws_abort.writeStr("abc");
    ASSERT_FALSE(ws_abort.flush());
    ASSERT_TRUE(ws_abort.aborted());
    ASSERT_EQ_INT(fail_out.chunk_sizes.size(), 0);
}

static void test_post_parser_and_clampers() {
    std::vector<std::pair<String, String>> params;
    const char body[] = "target=0&target=2&frames=250&empty=&ssids=Free+WiFi%0ATest%26Net%00Evil&reason=8";
    webParsePostBody(body, std::strlen(body), params);

    ASSERT_EQ_INT(params.size(), 6);
    ASSERT_STREQ(params[0].first, "target");
    ASSERT_STREQ(params[0].second, "0");
    ASSERT_STREQ(params[2].first, "frames");
    ASSERT_STREQ(params[2].second, "250");
    ASSERT_STREQ(params[3].first, "empty");
    ASSERT_STREQ(params[3].second, "");
    ASSERT_STREQ(params[4].first, "ssids");
    ASSERT_STREQ(params[4].second, "Free WiFi\nTest&NetEvil");

    // Parameter clamping must never confuse "" or "abc" with valid 0!
    ASSERT_EQ_INT(webClampFramesPerDeauth("", 100), 100);
    ASSERT_EQ_INT(webClampFramesPerDeauth("abc", 100), 100);
    ASSERT_EQ_INT(webClampFramesPerDeauth("0", 100), 100);
    ASSERT_EQ_INT(webClampFramesPerDeauth("5001", 100), 100);
    ASSERT_EQ_INT(webClampFramesPerDeauth("450", 100), 450);

    ASSERT_EQ_INT(webClampDeauthReason("0", 2), 2);
    ASSERT_EQ_INT(webClampDeauthReason("65536", 2), 2);
    ASSERT_EQ_INT(webClampDeauthReason("7", 2), 7);

    ASSERT_EQ_INT(webClampBeaconsPerSsid("0", 3), 3);
    ASSERT_EQ_INT(webClampBeaconsPerSsid("21", 3), 3);
    ASSERT_EQ_INT(webClampBeaconsPerSsid("12", 3), 12);

    ASSERT_EQ_INT(webClampRandomSsidCount("99"), 0);
    ASSERT_EQ_INT(webClampRandomSsidCount("16"), 16);

    ASSERT_EQ_INT(webClampBleSpamType("9"), 0);
    ASSERT_EQ_INT(webClampBleSpamType("2"), 2);
}

static void test_dashboard_ui_and_xss_protection() {
    resetWebState();

    WiFiScanResult ap24{};
    ap24.ssid = "<script>alert('xss')</script>";
    std::memcpy(ap24.bssid, "\x11\x22\x33\x44\x55\x66", 6);
    ap24.rssi = -52;
    ap24.channel = 6;
    g_scan_results.push_back(ap24);

    WiFiScanResult ap5{};
    ap5.ssid = "";
    std::memcpy(ap5.bssid, "\xAA\xBB\xCC\xDD\xEE\xFF", 6);
    ap5.rssi = -88;
    ap5.channel = 149;
    g_scan_results.push_back(ap5);

    BLEAuditResult ble{};
    ble.name = "AirTag<img>";
    std::snprintf(ble.address, sizeof(ble.address), "12:34:56:78:9A:BC");
    ble.addr_type = 1;
    ble.rssi = -60;
    g_ble_results.push_back(ble);

    g_app.notice.set(StatusKind::STATUS_OK, "Ready <ok> & steady");

    MemoryWebOutput out;
    const char req[] = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    CallbackRecorder rec;
    ASSERT_FALSE(webRouteRawRequest(req, std::strlen(req), out, testPostScanCb, testPrepareAttackCb, &rec));

    ASSERT_TRUE(out.max_chunk <= WEB_STREAM_CHUNK_SIZE);
    ASSERT_TRUE(out.contains("HTTP/1.1 200 OK\r\n"));
    // 4-tab layout & separated deauth / interfere formaction endpoints
    ASSERT_TRUE(out.contains("data-t='p_wifi'"));
    ASSERT_TRUE(out.contains("data-t='p_spam'"));
    ASSERT_TRUE(out.contains("data-t='p_ble'"));
    ASSERT_TRUE(out.contains("data-t='p_cfg'"));
    ASSERT_TRUE(out.contains("formaction='/deauth'"));
    ASSERT_TRUE(out.contains("formaction='/interfere'"));
    ASSERT_TRUE(out.contains("sessionStorage.getItem('tab')"));
    // XSS escaping in AP SSID, Notice banner, and BLE Name; raw tags must NOT appear!
    ASSERT_FALSE(out.contains("<script>alert('xss')</script>"));
    ASSERT_TRUE(out.contains("&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;"));
    ASSERT_TRUE(out.contains("Ready &lt;ok&gt; &amp; steady"));
    ASSERT_TRUE(out.contains("AirTag&lt;img&gt;"));
    ASSERT_TRUE(out.contains("*Hidden Network*"));
    ASSERT_TRUE(out.contains("CH 6 &middot; 11:22:33:44:55:66"));
    ASSERT_TRUE(out.contains("CH 149 &middot; AA:BB:CC:DD:EE:FF"));
}

static void test_http_router_endpoints() {
    resetWebState();
    CallbackRecorder rec;

    // Seed 2 scanned APs
    for (int i = 0; i < 2; i++) {
        WiFiScanResult ap{};
        ap.ssid = (i == 0) ? "Net24" : "Net5G";
        std::memset(ap.bssid, static_cast<uint8_t>(0x10 + i), 6);
        ap.channel = (i == 0) ? 1 : 36;
        ap.rssi = -55;
        g_scan_results.push_back(ap);
    }

    // 204 favicon, 404 unknown, 405 GET on POST endpoint, 400 malformed
    {
        MemoryWebOutput out;
        const char r[] = "GET /favicon.ico HTTP/1.1\r\n\r\n";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(out.contains("HTTP/1.1 204 No Content\r\n"));
    }
    {
        MemoryWebOutput out;
        const char r[] = "GET /unknown HTTP/1.1\r\n\r\n";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(out.contains("HTTP/1.1 404 Not Found\r\n"));
    }
    {
        MemoryWebOutput out;
        const char r[] = "GET /deauth HTTP/1.1\r\n\r\n";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(out.contains("HTTP/1.1 405 Method Not Allowed\r\n"));
    }
    {
        MemoryWebOutput out;
        const char r[] = "JUNK";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(out.contains("HTTP/1.1 400 Bad Request\r\n"));
    }

    // POST /rescan invokes scan callback and sets STATUS_OK toast notice
    {
        MemoryWebOutput out;
        const char r[] = "POST /rescan HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_EQ_INT(rec.scan_calls, 1);
        ASSERT_TRUE(g_app.notice.kind == StatusKind::STATUS_OK);
        ASSERT_TRUE(out.contains("HTTP/1.1 303 See Other\r\n"));
    }

    // POST /deauth without targets must stay IDLE with STATUS_WARN toast
    {
        MemoryWebOutput out;
        const char r[] = "POST /deauth HTTP/1.1\r\nContent-Length: 10\r\n\r\nframes=200";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(g_app.attack_mode == AttackMode::IDLE);
        ASSERT_TRUE(g_app.notice.kind == StatusKind::STATUS_WARN);
    }

    // POST /deauth with valid targets starts DEAUTH and populates g_deauth_channels
    {
        MemoryWebOutput out;
        const char r[] = "POST /deauth HTTP/1.1\r\n\r\ntarget=0&target=1&frames=300&reason=7";
        ASSERT_TRUE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(g_app.attack_mode == AttackMode::DEAUTH);
        ASSERT_EQ_INT(g_app.config.frames_per_deauth, 300);
        ASSERT_EQ_INT(g_app.config.deauth_reason, 7);
        ASSERT_EQ_INT(g_deauth_channels[1].size(), 1);
        ASSERT_EQ_INT(g_deauth_channels[36].size(), 1);
        ASSERT_EQ_INT(rec.prepare_attack_calls, 1);
    }

    // POST /interfere switches into INTERFERENCE cleanly
    resetWebState();
    for (int i = 0; i < 2; i++) {
        WiFiScanResult ap{};
        ap.ssid = "Target";
        std::memset(ap.bssid, static_cast<uint8_t>(0x20 + i), 6);
        ap.channel = 6;
        g_scan_results.push_back(ap);
    }
    {
        MemoryWebOutput out;
        const char r[] = "POST /interfere HTTP/1.1\r\n\r\ntarget=0&target=1";
        ASSERT_TRUE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(g_app.attack_mode == AttackMode::INTERFERENCE);
        ASSERT_TRUE(get_interference_state() != INT_STATE_IDLE);
        stop_interference();
    }

    // POST /spam starts BEACON_SPAM from custom SSID list and warns when empty
    resetWebState();
    {
        MemoryWebOutput out;
        const char r[] = "POST /spam HTTP/1.1\r\n\r\nssids=&random_count=0";
        ASSERT_FALSE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(g_app.attack_mode == AttackMode::IDLE);
        ASSERT_TRUE(g_app.notice.kind == StatusKind::STATUS_WARN);
    }
    {
        MemoryWebOutput out;
        const char r[] = "POST /spam HTTP/1.1\r\n\r\nssids=Alpha+AP%0D%0ABeta+AP&count=2&random_count=0";
        ASSERT_TRUE(webRouteRawRequest(r, std::strlen(r), out, testPostScanCb, testPrepareAttackCb, &rec));
        ASSERT_TRUE(g_app.attack_mode == AttackMode::BEACON_SPAM);
        ASSERT_TRUE(Spammer.isActive());
        ASSERT_EQ_INT(Spammer.getNetworkCount(), 4);
        Spammer.stop();
