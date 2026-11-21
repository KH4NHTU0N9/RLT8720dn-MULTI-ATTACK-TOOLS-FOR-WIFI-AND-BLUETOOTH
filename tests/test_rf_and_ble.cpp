#include <Arduino.h>
#include "wifi_conf.h"
#include "BLEDevice.h"
#undef min
#undef max
#include "ble/ble_manager.h"
#include "config/app_config.h"
#include "core/app_state.h"
#include "core/attack_runner.h"
#include "core/led_controller.h"
#include "interference/interference.h"
#include "oled/oled_display.h"
#include "packet-injection/packet-injection.h"
#include "spammer/spammer.h"
#include "wifi/wifi_scanner.h"

#include <cstdio>
#include <cstring>
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
static unsigned char g_wlan_running_flag = 1;
static std::vector<std::vector<uint8_t>> g_tx_frames;

extern "C" {
unsigned char rltk_wlan_running(unsigned char idx) {
    (void)idx;
    return g_wlan_running_flag;
}
void* alloc_mgtxmitframe(void* ptr) { (void)ptr; return nullptr; }
void update_mgntframe_attrib(void* ptr, void* fc) { (void)ptr; (void)fc; }
int dump_mgntframe(void* ptr, void* fc) { (void)ptr; (void)fc; return 0; }
bool unit_test_capture_raw_frame(const void* frame, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(frame);
    g_tx_frames.emplace_back(bytes, bytes + length);
    return true;
}
}

static void resetHarness() {
    mockArduino().reset();
    ledInit();
    mockWifi().reset();
    mockBleInstance().reset();
    g_tx_frames.clear();
    g_wlan_running_flag = 1;
    rltk_wlan_info = g_fake_wlan_storage;
    g_app = AppState{};
    g_scan_results.clear();
    g_ble_results.clear();
    g_deauth_channels.clear();
    stop_interference();
    Spammer.stop();
    Spammer.clearSSIDs();
}

static const uint8_t* findTaggedIe(const uint8_t* ie_start, size_t ie_len, uint8_t target_id) {
    size_t pos = 0;
    while (pos + 2 <= ie_len) {
        uint8_t id = ie_start[pos];
        uint8_t len = ie_start[pos + 1];
        if (pos + 2 + len > ie_len) return nullptr;
        if (id == target_id) return &ie_start[pos];
        pos += 2 + len;
    }
    return nullptr;
}

static void test_packet_injection_frames_and_guards() {
    resetHarness();

    // Verify packed IEEE 802.11 management/control frame sizes
    ASSERT_EQ_INT(sizeof(DeauthFrame), 26);
    ASSERT_EQ_INT(sizeof(DisassocFrame), 26);
    ASSERT_EQ_INT(sizeof(AuthFrame), 30);
    ASSERT_EQ_INT(sizeof(AssocFrame), 28);
    ASSERT_EQ_INT(sizeof(NullFrame), 24); // Fixed: non-QoS 0x48 must not carry QoS Control
    ASSERT_EQ_INT(sizeof(ProbeRequest), 24);
    ASSERT_EQ_INT(sizeof(CSAFrame), 31); // Fixed: packed Switch Count byte included

    // Raw frame boundary and radio state guards
    uint8_t dummy[32] = {0};
    ASSERT_FALSE(wifi_tx_raw_frame(nullptr, 24));
    ASSERT_FALSE(wifi_tx_raw_frame(dummy, 23));
    ASSERT_FALSE(wifi_tx_raw_frame(dummy, 1537));
    g_wlan_running_flag = 0;
    ASSERT_FALSE(wifi_tx_raw_frame(dummy, 24));
    g_wlan_running_flag = 1;
    rltk_wlan_info = nullptr;
    ASSERT_FALSE(wifi_tx_raw_frame(dummy, 24));
    rltk_wlan_info = g_fake_wlan_storage;
    ASSERT_TRUE(wifi_tx_raw_frame(dummy, 24));

