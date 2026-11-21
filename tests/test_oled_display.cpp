#include <Arduino.h>
#include "oled/oled_display.h"

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

struct Ssd1306WireListener {
    static constexpr uint8_t PIN_SCL = 7;
    static constexpr uint8_t PIN_SDA = 8;

    uint8_t prev_scl = 1;
    uint8_t prev_sda = 1;
    bool in_tx = false;
    int bit_idx = 0;
    uint8_t cur_byte = 0;
    int byte_pos = 0;
    uint8_t ctrl_byte = 0;
    uint8_t cmd_wait = 0;
    uint8_t pending_cmd = 0;
    uint8_t cmd_args[2] = {0, 0};
    uint8_t cmd_arg_pos = 0;

