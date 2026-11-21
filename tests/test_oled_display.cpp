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

    uint8_t col_start = 0;
    uint8_t col_end = 127;
    uint8_t page_start = 0;
    uint8_t page_end = 7;
    uint8_t cur_col = 0;
    uint8_t cur_page = 0;

    uint8_t gddram[128 * 8] = {0};
    std::vector<uint8_t> commands;
    size_t tx_count = 0;
    size_t wrong_addr_count = 0;
    size_t data_bytes_written = 0;
    size_t data_pages_written = 0;

    void resetCounters() {
        commands.clear();
        data_bytes_written = 0;
        data_pages_written = 0;
        tx_count = 0;
    }

    static bool isPixelOn(const uint8_t ram[128 * 8], int x, int y) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return false;
        size_t idx = static_cast<size_t>(y >> 3) * 128U + static_cast<size_t>(x);
        return (ram[idx] & (1U << (y & 7))) != 0;
    }

    int countPixelsInRect(int x0, int y0, int x1, int y1) const {
        int cnt = 0;
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                if (isPixelOn(gddram, x, y)) cnt++;
            }
        }
        return cnt;
    }

    void onByte(uint8_t b) {
        if (byte_pos == 0) {
            if (b != (OLED_I2C_ADDR << 1)) {
                wrong_addr_count++;
            }
        } else if (byte_pos == 1) {
            ctrl_byte = b;
            if (ctrl_byte == 0x40) {
                cur_col = col_start;
                cur_page = page_start;
                data_pages_written++;
            }
        } else if (ctrl_byte == 0x00) {
            commands.push_back(b);
            if (cmd_wait > 0) {
                cmd_args[cmd_arg_pos++] = b;
                cmd_wait--;
                if (cmd_wait == 0) {
                    if (pending_cmd == 0x21) {
                        col_start = cmd_args[0] & 0x7FU;
                        col_end = cmd_args[1] & 0x7FU;
                    } else if (pending_cmd == 0x22) {
                        page_start = cmd_args[0] & 0x07U;
                        page_end = cmd_args[1] & 0x07U;
                    }
                    cur_col = col_start;
                    cur_page = page_start;
                }
            } else if (b == 0x21 || b == 0x22) {
                pending_cmd = b;
                cmd_wait = 2;
                cmd_arg_pos = 0;
            }
        } else if (ctrl_byte == 0x40) {
            size_t idx = static_cast<size_t>(cur_page) * 128U + static_cast<size_t>(cur_col);
            if (idx < sizeof(gddram)) {
                gddram[idx] = b;
            }
            data_bytes_written++;
            if (cur_col < col_end) {
                cur_col++;
            } else {
                cur_col = col_start;
                if (cur_page < page_end) cur_page++;
                else cur_page = page_start;
            }
        }
        byte_pos++;
    }

    static void gpioHook(uint8_t pin, uint8_t mode, uint8_t level, void* ctx) {
        (void)pin;
        (void)mode;
        (void)level;
        Ssd1306WireListener* self = static_cast<Ssd1306WireListener*>(ctx);
        const MockArduinoState& st = mockArduino();
        const uint8_t scl = st.pin_level[PIN_SCL];
        const uint8_t sda = (st.pin_mode[PIN_SDA] == OUTPUT) ? st.pin_level[PIN_SDA] : 1;

        if (self->prev_scl == 1 && scl == 1 && self->prev_sda == 1 && sda == 0) {
            self->in_tx = true;
            self->bit_idx = 0;
            self->cur_byte = 0;
            self->byte_pos = 0;
            self->tx_count++;
        } else if (self->prev_scl == 1 && scl == 1 && self->prev_sda == 0 && sda == 1) {
            self->in_tx = false;
        } else if (self->prev_scl == 0 && scl == 1 && self->in_tx) {
            if (self->bit_idx < 8) {
                self->cur_byte = static_cast<uint8_t>((self->cur_byte << 1) | (sda & 1U));
                self->bit_idx++;
            } else {
                self->bit_idx = 0;
                self->onByte(self->cur_byte);
                self->cur_byte = 0;
            }
        }

        self->prev_scl = scl;
        self->prev_sda = sda;
    }
};

static void test_pre_init_guards_and_gpio_hardfault_fix() {
    mockArduino().reset();
    Ssd1306WireListener listener;
    mockArduino().on_gpio_event = Ssd1306WireListener::gpioHook;
    mockArduino().on_gpio_ctx = &listener;

    // Calling render functions before oled_init() must safely no-op without touching GPIO
    ASSERT_FALSE(oled_is_initialized());
    oled_show_boot();
    oled_show_scanning();
    oled_show_idle(1, 1);
    oled_show_spammer(5);
    ASSERT_EQ_INT(mockArduino().digital_write_calls, 0);

    // Initialize OLED and verify pinMode ALWAYS precedes digitalWrite (prevents AmebaD HardFault)
    ASSERT_TRUE(oled_init());
    ASSERT_TRUE(oled_is_initialized());
    ASSERT_EQ_INT(mockArduino().write_before_pinmode_violations, 0);
    ASSERT_EQ_INT(listener.wrong_addr_count, 0);

    // Must send Display OFF (0xAE) ... Display ON (0xAF) and clear all 8 pages (1024 zero bytes)
    ASSERT_TRUE(!listener.commands.empty());
    ASSERT_EQ_INT(listener.commands.front(), 0xAE);
    ASSERT_EQ_INT(listener.data_pages_written, 8);
    ASSERT_EQ_INT(listener.data_bytes_written, 1024);
    ASSERT_EQ_INT(listener.countPixelsInRect(0, 0, 127, 63), 0);
}

static void test_dirty_page_diffing_and_screens() {
    mockArduino().reset();
    Ssd1306WireListener listener;
    mockArduino().on_gpio_event = Ssd1306WireListener::gpioHook;
    mockArduino().on_gpio_ctx = &listener;

    ASSERT_TRUE(oled_init());
    listener.resetCounters();

    // Draw Idle screen: header is y=0..11, gap row y=12..13 must be dark, separator line y=14 full width
    oled_show_idle(4, 2);
    ASSERT_EQ_INT(mockArduino().write_before_pinmode_violations, 0);
    ASSERT_EQ_INT(listener.wrong_addr_count, 0);
    ASSERT_EQ_INT(listener.countPixelsInRect(0, 12, 127, 13), 0);
    ASSERT_EQ_INT(listener.countPixelsInRect(0, 14, 127, 14), 128);
    ASSERT_TRUE(listener.data_pages_written > 0);

    // Re-drawing identical Idle screen must skip ALL 8 pages via dirty-page diffing!
    listener.resetCounters();
    oled_show_idle(4, 2);
    ASSERT_EQ_INT(listener.data_pages_written, 0);
    ASSERT_EQ_INT(listener.data_bytes_written, 0);

