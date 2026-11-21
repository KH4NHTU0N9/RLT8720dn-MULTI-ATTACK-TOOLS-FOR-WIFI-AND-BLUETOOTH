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

