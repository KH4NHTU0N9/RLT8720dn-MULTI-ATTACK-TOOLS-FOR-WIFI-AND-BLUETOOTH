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

    uint8_t src[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
    uint8_t dst[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // NAV Deauth must clamp Duration to IEEE80211_MAX_NAV_DURATION (0x7FFF, bit 15 = 0)
    g_tx_frames.clear();
    ASSERT_TRUE(wifi_tx_deauth_nav(src, dst, 0x07));
    ASSERT_EQ_INT(g_tx_frames.size(), 1);
    ASSERT_EQ_INT(g_tx_frames[0][0], 0xC0);
    uint16_t nav_dur = 0;
    std::memcpy(&nav_dur, &g_tx_frames[0][2], 2);
    ASSERT_EQ_INT(nav_dur, 0x7FFF);

    // Beacon must embed non-zero TSF and Supported Rates IE
    g_tx_frames.clear();
    mockAdvanceMillis(42);
    ASSERT_TRUE(wifi_tx_beacon_frame(src, dst, "AuditNet"));
    ASSERT_EQ_INT(g_tx_frames.size(), 1);
    uint64_t tsf = 0;
    std::memcpy(&tsf, &g_tx_frames[0][24], 8);
    ASSERT_EQ_INT(tsf, 42000ULL);
    const uint8_t* ies = &g_tx_frames[0][36];
    const size_t ies_len = g_tx_frames[0].size() - 36;
    ASSERT_TRUE(findTaggedIe(ies, ies_len, 0x00) != nullptr);
    ASSERT_TRUE(findTaggedIe(ies, ies_len, 0x01) != nullptr);

    // 2.4 GHz CSA Beacon (ch 6): Tag 3 (DS), Tag 37 (CSA, len=3), Tag 60 (ECSA, len=4, op=81)
    g_tx_frames.clear();
    ASSERT_TRUE(wifi_tx_beacon_csa_frame(src, dst, "Test24", 6));
    const auto& csa24 = g_tx_frames[0];
    const uint8_t* ds24 = findTaggedIe(&csa24[36], csa24.size() - 36, 3);
    const uint8_t* e37_24 = findTaggedIe(&csa24[36], csa24.size() - 36, 37);
    const uint8_t* e60_24 = findTaggedIe(&csa24[36], csa24.size() - 36, 60);
    ASSERT_TRUE(ds24 != nullptr && ds24[2] == 6);
    ASSERT_TRUE(e37_24 != nullptr && e37_24[1] == 3 && e37_24[3] == 6);
    ASSERT_TRUE(e60_24 != nullptr && e60_24[1] == 4 && e60_24[2] == 1 && e60_24[3] == 81 && e60_24[4] == 6);

    // 5 GHz CSA Beacons: MUST OMIT Tag 3 (DS Param) and set exact IEEE Operating Class!
    const struct { uint8_t ch; uint8_t op; } cases5g[] = {
        {36, 115}, {52, 118}, {100, 121}, {149, 124}
    };
    for (size_t i = 0; i < sizeof(cases5g) / sizeof(cases5g[0]); i++) {
        g_tx_frames.clear();
        ASSERT_TRUE(wifi_tx_beacon_csa_frame(src, dst, "Test5G", cases5g[i].ch));
        const auto& csa5 = g_tx_frames[0];
        ASSERT_TRUE(findTaggedIe(&csa5[36], csa5.size() - 36, 3) == nullptr);
        const uint8_t* e60 = findTaggedIe(&csa5[36], csa5.size() - 36, 60);
        ASSERT_TRUE(e60 != nullptr);
        ASSERT_EQ_INT(e60[1], 4);
        ASSERT_EQ_INT(e60[3], cases5g[i].op);
        ASSERT_EQ_INT(e60[4], cases5g[i].ch);
        ASSERT_EQ_INT(e60[5], 1);
    }
}

static void test_spammer_module() {
    resetHarness();

    Spammer.addSSID("");
    ASSERT_EQ_INT(Spammer.getNetworkCount(), 0);
    Spammer.addSSID("ValidSSID_12345678901234567890_OVERLONG_SUFFIX");
    ASSERT_EQ_INT(Spammer.getNetworkCount(), 1);

    // Enforces MAX_SPAM_NETWORKS (64)
    for (int i = 0; i < 80; i++) {
        Spammer.addSSID(String("Net_") + String(i));
    }
    ASSERT_EQ_INT(Spammer.getNetworkCount(), MAX_SPAM_NETWORKS);

    // Random mode clamps count [1..64], picks LAA + unicast MACs, and emits valid beacons
    Spammer.clearSSIDs();
    Spammer.setRandomMode(true, 100);
    Spammer.start();
    ASSERT_TRUE(Spammer.isActive());
    ASSERT_EQ_INT(Spammer.getNetworkCount(), MAX_SPAM_NETWORKS);

    g_tx_frames.clear();
    uint32_t sent = Spammer.run();
    ASSERT_EQ_INT(sent, MAX_SPAM_NETWORKS);
    ASSERT_EQ_INT(g_tx_frames.size(), MAX_SPAM_NETWORKS);

