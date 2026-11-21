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

    const auto& f = g_tx_frames[0];
    ASSERT_EQ_INT(f[0], 0x80);
    ASSERT_TRUE(f.size() <= MAX_BEACON_SIZE);
    // SA/BSSID must be unicast + locally administered ((mac[0] & 0x03) == 0x02)
    ASSERT_EQ_INT(f[10] & 0x03U, 0x02);
    const uint8_t* ssid_ie = findTaggedIe(&f[36], f.size() - 36, 0);
    const uint8_t* rate_ie = findTaggedIe(&f[36], f.size() - 36, 1);
    const uint8_t* ds_ie = findTaggedIe(&f[36], f.size() - 36, 3);
    ASSERT_TRUE(ssid_ie != nullptr && ssid_ie[1] >= 8 && ssid_ie[1] <= MAX_SSID_LEN);
    ASSERT_TRUE(rate_ie != nullptr);
    ASSERT_TRUE(ds_ie != nullptr && ds_ie[2] >= 1 && ds_ie[2] <= 13);

    Spammer.stop();
    ASSERT_FALSE(Spammer.isActive());
}

static void test_interference_sniffer_deadlock_and_synthetic_fallback() {
    resetHarness();

    // TLV sanitizer rejects truncated/corrupt IEs while keeping valid complete IEs
    const uint8_t valid_plus_trunc[] = {
        0x00, 0x03, 'A', 'P', '1',
        0x03, 0x01, 0x06,
        0x3D, 0x16, 0x00, 0x01 // truncated HT Operation claiming 22 bytes
    };
    ASSERT_EQ_INT(interference_sanitize_beacon_tags(valid_plus_trunc, sizeof(valid_plus_trunc), 400), 8);
    ASSERT_EQ_INT(interference_sanitize_beacon_tags(nullptr, 10, 400), 0);

    uint8_t target_mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    start_interference(target_mac, 36);
    ASSERT_EQ_INT(get_interference_state(), INT_STATE_CAPTURING);
    ASSERT_EQ_INT(mockWifi().promisc_level, RTW_PROMISC_ENABLE_2);
    ASSERT_EQ_INT(mockWifi().promisc_len_used, 1);
    ASSERT_TRUE(mockWifi().promisc_cb != nullptr);

    // Feed bogus non-beacon and wrong-BSSID frames -> must NOT capture
    uint8_t wrong_fc[64] = {0};
    wrong_fc[0] = 0xC0; // deauth, not beacon
    std::memcpy(&wrong_fc[16], target_mac, 6);
    mockWifiInjectPromiscFrame(wrong_fc, sizeof(wrong_fc));
    ASSERT_EQ_INT(mockWifi().self_deadlock_detected, false);
    ASSERT_FALSE(is_captured());

    uint8_t wrong_bssid[64] = {0};
    wrong_bssid[0] = 0x80;
    wrong_bssid[1] = 0x00;
    wrong_bssid[36] = 0x00; wrong_bssid[37] = 0x02; wrong_bssid[38] = 'H'; wrong_bssid[39] = 'I';
    mockWifiInjectPromiscFrame(wrong_bssid, 40);
    ASSERT_FALSE(is_captured());

    // Valid beacon from target_mac MUST stage without calling wifi_set_promisc() inside callback!
    uint8_t real_beacon[48] = {0};
    real_beacon[0] = 0x80;
    real_beacon[1] = 0x00;
    std::memcpy(&real_beacon[16], target_mac, 6);
    real_beacon[32] = 0x64; real_beacon[33] = 0x00;
    real_beacon[34] = 0x11; real_beacon[35] = 0x04;
    real_beacon[36] = 0x00; real_beacon[37] = 0x04;
    real_beacon[38] = 'V'; real_beacon[39] = 'I'; real_beacon[40] = 'P'; real_beacon[41] = '5';
    real_beacon[42] = 0x01; real_beacon[43] = 0x02; real_beacon[44] = 0x8C; real_beacon[45] = 0x12;
    mockWifiInjectPromiscFrame(real_beacon, 46);
    ASSERT_FALSE(mockWifi().self_deadlock_detected);

    // Main loop finalizes capture and disarms promisc safely outside RX thread
    uint32_t sent = run_interference_cycle();
    ASSERT_TRUE(sent > 0);
    ASSERT_TRUE(is_captured());
    ASSERT_EQ_INT(get_interference_state(), INT_STATE_ACTIVE);
    ASSERT_EQ_INT(mockWifi().promisc_level, RTW_PROMISC_DISABLE);

    // Synthetic timeout fallback on both 2.4 GHz (ch 11) and 5 GHz (ch 149) + multi-target bounds
    resetHarness();
    uint8_t mac24[6] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x24};
    uint8_t mac5g[6] = {0xBB, 0x00, 0x00, 0x00, 0x00, 0x5A};
    start_interference_multi();
    ASSERT_FALSE(add_interference_target(nullptr, 6));
    ASSERT_FALSE(add_interference_target(mac24, 0));
    ASSERT_TRUE(add_interference_target(mac24, 11));
    ASSERT_TRUE(add_interference_target(mac5g, 149));
    ASSERT_TRUE(add_interference_target(mac24, 1));
    ASSERT_TRUE(add_interference_target(mac5g, 36));
    ASSERT_FALSE(add_interference_target(mac24, 6)); // 5th rejected

    // Force capture timeout on target 0 -> falls back cleanly to synthetic tpl_24
    mockAdvanceMillis(3000);
    g_tx_frames.clear();
    uint32_t syn_sent = run_interference_cycle();
    ASSERT_TRUE(syn_sent > 0);
    ASSERT_EQ_INT(get_active_target_count(), 4);
    stop_interference();
}

static void test_led_and_wifi_scanner() {
    resetHarness();

    // Global LED disable must silence all subsequent LED transitions
    ledSetEnabled(false);
    const uint32_t writes_before = mockArduino().digital_write_calls;
    ledShowHttpBusy();
    ledSetBlue(true);
    ledToggleGreen();
    ASSERT_EQ_INT(mockArduino().digital_write_calls, writes_before);
    ledSetEnabled(true);
    ledShowIdle();
    ASSERT_EQ_INT(mockArduino().pin_level[LED_R], HIGH);
    ASSERT_EQ_INT(mockArduino().pin_level[LED_G], LOW);
    ASSERT_EQ_INT(mockArduino().pin_level[LED_B], LOW);
    ASSERT_EQ_INT(mockArduino().write_before_pinmode_violations, 0);

    // Wi-Fi scanner handles sanitized SSIDs, MAX_SCAN_RESULTS, and consistent band counts (ch 15..35 not dropped!)
    wifiScannerInit();
    for (int i = 0; i < 70; i++) {
        rtw_scan_result_t ap{};
        std::memset(&ap, 0, sizeof(ap));
        ap.channel = (i == 0) ? 11 : (i == 1) ? 20 : 149; // ch 20 must count in 5G without gap
        ap.signal_strength = -55;
        ap.BSSID.octet[5] = static_cast<uint8_t>(i);
        std::memcpy(ap.SSID.val, "Cafe\x01Net", 8);
        ap.SSID.len = 8;
        mockWifi().canned_scan_results.push_back(ap);
    }

    ASSERT_EQ_INT(wifiScanNetworks(), 0);
    ASSERT_EQ_INT(g_scan_results.size(), MAX_SCAN_RESULTS);
    ASSERT_STREQ(g_scan_results[0].ssid, "CafeNet");
    int c24 = 0, c5g = 0;
    wifiScanCountByBand(&c24, &c5g);
    ASSERT_EQ_INT(c24, 1);
    ASSERT_EQ_INT(c5g, MAX_SCAN_RESULTS - 1);
    ASSERT_EQ_INT(c24 + c5g, MAX_SCAN_RESULTS);

    mockWifi().wifi_scan_should_fail = true;
    ASSERT_EQ_INT(wifiScanNetworks(), 1);
}

static void test_ble_manager_payloads_scan_and_gatt_write() {
    resetHarness();

    // Apple Proximity packet: AD1 (02 01 1A) + AD2 (len 0x1A, type 0xFF, 25 data bytes) = 30B exact
    uint8_t buf[32];
    size_t n = bleBuildAppleSpamPayload(buf, sizeof(buf), 1, 0xFE, 0xFF);
    ASSERT_EQ_INT(n, BLE_APPLE_PAYLOAD_LEN);
    ASSERT_EQ_INT(buf[0], 0x02);
    ASSERT_EQ_INT(buf[1], 0x01);
    ASSERT_EQ_INT(buf[2], 0x1A);
    ASSERT_EQ_INT(buf[3], 0x1A);
    ASSERT_EQ_INT(buf[4], 0xFF);
    ASSERT_EQ_INT((1U + buf[0]) + (1U + buf[3]), n);
    ASSERT_EQ_INT(buf[13], 0xFE);
    ASSERT_EQ_INT(buf[14], 0xFF);

    ASSERT_EQ_INT(bleBuildGoogleSpamPayload(buf, sizeof(buf), 0), BLE_GOOGLE_PAYLOAD_LEN);
    const uint8_t r4[4] = {0x11, 0x22, 0x33, 0x44};
    ASSERT_EQ_INT(bleBuildWindowsSpamPayload(buf, sizeof(buf), r4), BLE_WINDOWS_PAYLOAD_LEN);
    ASSERT_EQ_INT(bleBuildAppleSpamPayload(nullptr, sizeof(buf), 0, 0, 0), 0);
    ASSERT_EQ_INT(bleBuildAppleSpamPayload(buf, 10, 0, 0, 0), 0);

    // BLE Scan: single blocking duration (no double-delay), reverses bd_addr, caps at MAX_BLE_RESULTS
    bleManagerInit();
    for (int i = 0; i < 25; i++) {
        T_LE_SCAN_INFO info{};
        std::memset(&info, 0, sizeof(info));
        info.bd_addr[5] = 0xAA;
        info.bd_addr[0] = static_cast<uint8_t>(i);
        info.remote_addr_type = (i % 2);
        info.rssi = -65;
        const uint8_t adv[] = {0x05, 0x09, 'D', 'e', 'v', static_cast<uint8_t>('0' + (i % 10))};
        std::memcpy(info.data, adv, sizeof(adv));
        info.data_len = sizeof(adv);
        mockBleInstance().scan.canned_devices.push_back(info);
    }

