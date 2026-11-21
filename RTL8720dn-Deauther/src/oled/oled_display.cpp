#include "oled_display.h"
#include <stdio.h>
#include <string.h>

// ===================================================================
// Self-Contained SSD1306 OLED Driver for BW16 (RTL8720DN)
// Uses Software I2C with dirty-page diffing and cached GPIO states.
// ===================================================================

#define OLED_SDA  8    // PA26 = Arduino pin 8 on BW16
#define OLED_SCL  7    // PA25 = Arduino pin 7 on BW16
#define OLED_FB_BYTES (OLED_SCREEN_WIDTH * OLED_PAGE_COUNT)

// --- State ---
static uint8_t  _fb[OLED_FB_BYTES];
static uint8_t  _prev_fb[OLED_FB_BYTES];
static bool     _force_redraw = true;
static bool     _initialized  = false;
static uint32_t _last_refresh = 0;
static uint32_t _attack_start = 0;
static uint8_t  _anim_frame   = 0;
static bool     _sda_is_out   = false;
static bool     _scl_is_high  = true;
static const char _spinner[]  = { '|', '/', '-', '\\' };

// ===================================================================
// SOFTWARE I2C (bit-bang, cached pin state for low overhead)
// ===================================================================
static void _i2c_dly() { delayMicroseconds(2); }

static void _sda_high() {
    if (_sda_is_out) {
        pinMode(OLED_SDA, INPUT_PULLUP);
        _sda_is_out = false;
    }
}

static void _sda_low() {
    if (!_sda_is_out) {
        pinMode(OLED_SDA, OUTPUT);
        digitalWrite(OLED_SDA, LOW);
        _sda_is_out = true;
    }
}

static void _scl_high() {
    if (!_scl_is_high) {
        digitalWrite(OLED_SCL, HIGH);
        _scl_is_high = true;
    }
}

static void _scl_low() {
    if (_scl_is_high) {
        digitalWrite(OLED_SCL, LOW);
        _scl_is_high = false;
    }
}

static void _i2c_init() {
    // AmebaD allocates gpio_pin_struct[pin] inside pinMode(); initialize before digitalWrite().
    pinMode(OLED_SDA, OUTPUT);
    digitalWrite(OLED_SDA, LOW);
    _sda_is_out = true;

    pinMode(OLED_SCL, OUTPUT);
    digitalWrite(OLED_SCL, HIGH);
    _scl_is_high = true;

    _sda_high();
    _i2c_dly();
}

static void _i2c_start() {
    _sda_high(); _i2c_dly();
    _scl_high(); _i2c_dly();
    _sda_low();  _i2c_dly();
    _scl_low();  _i2c_dly();
}

static void _i2c_stop() {
    _sda_low();  _i2c_dly();
    _scl_high(); _i2c_dly();
    _sda_high(); _i2c_dly();
}

static void _i2c_write(uint8_t d) {
    for (int i = 7; i >= 0; i--) {
        if (d & (uint8_t)(1U << i)) _sda_high();
        else                        _sda_low();
        _i2c_dly();
        _scl_high(); _i2c_dly();
        _scl_low();  _i2c_dly();
    }
    // 9th clock cycle for ACK (ignored if no display attached)
    _sda_high(); _i2c_dly();
    _scl_high(); _i2c_dly();
    _scl_low();  _i2c_dly();
}

static void _i2c_begin() {
    _i2c_start();
    _i2c_write((uint8_t)(OLED_I2C_ADDR << 1));
}

// ===================================================================
// SSD1306 LOW-LEVEL
// ===================================================================
static void _ssd1306_init_hw() {
    static const uint8_t init[] = {
        0xAE,        // Display OFF
        0xD5, 0x80,  // Clock divide ratio
        0xA8, 0x3F,  // Multiplex ratio: 63 (64 rows)
        0xD3, 0x00,  // Display offset: 0
        0x40,        // Start line: 0
        0x8D, 0x14,  // Charge pump: enable
        0x20, 0x00,  // Addressing: horizontal
        0xA1,        // Segment remap
        0xC8,        // COM scan direction
        0xDA, 0x12,  // COM pins config
        0x81, 0xCF,  // Contrast
        0xD9, 0xF1,  // Pre-charge period
        0xDB, 0x40,  // VCOMH deselect
        0xA4,        // Display from RAM
        0xA6,        // Normal (not inverted)
        0xAF,        // Display ON
    };
    _i2c_begin();
    _i2c_write(0x00);
    for (size_t i = 0; i < sizeof(init); i++) {
        _i2c_write(init[i]);
    }
    _i2c_stop();
}

static void _display() {
    for (int p = 0; p < OLED_PAGE_COUNT; p++) {
        const size_t offset = (size_t)p * OLED_SCREEN_WIDTH;
        if (!_force_redraw && memcmp(&_fb[offset], &_prev_fb[offset], OLED_SCREEN_WIDTH) == 0) {
            continue;
        }
        // Address the dirty page directly in horizontal addressing mode
        _i2c_begin(); _i2c_write(0x00);
        _i2c_write(0x21); _i2c_write(0); _i2c_write((uint8_t)(OLED_SCREEN_WIDTH - 1));
        _i2c_write(0x22); _i2c_write((uint8_t)p); _i2c_write((uint8_t)p);
        _i2c_stop();

        _i2c_begin();
        _i2c_write(0x40);
        for (int c = 0; c < OLED_SCREEN_WIDTH; c++) {
            _i2c_write(_fb[offset + (size_t)c]);
        }
        _i2c_stop();

        memcpy(&_prev_fb[offset], &_fb[offset], OLED_SCREEN_WIDTH);
    }
    _force_redraw = false;
}

// ===================================================================
// FRAMEBUFFER & PIXEL OPS
// ===================================================================
static void _clear() { memset(_fb, 0, sizeof(_fb)); }

static void _pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_SCREEN_WIDTH || y < 0 || y >= OLED_SCREEN_HEIGHT) return;
    const size_t idx = (size_t)(y >> 3) * OLED_SCREEN_WIDTH + (size_t)x;
    const uint8_t mask = (uint8_t)(1U << (y & 7));
    if (on) _fb[idx] |= mask;
    else    _fb[idx] &= (uint8_t)~mask;
}

// ===================================================================
// 5x7 FONT (ASCII 32-126, 475 bytes)
// ===================================================================
static const uint8_t _font[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, // SP !
    0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14, // "  #
    0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, // $  %
    0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00, // &  '
    0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, // (  )
    0x08,0x2A,0x1C,0x2A,0x08, 0x08,0x08,0x3E,0x08,0x08, // *  +
    0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, // ,  -
    0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02, // .  /
    0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, // 0  1
    0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31, // 2  3
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, // 4  5
    0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03, // 6  7
    0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, // 8  9
    0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00, // :  ;
    0x00,0x08,0x14,0x22,0x41, 0x14,0x14,0x14,0x14,0x14, // <  =
    0x41,0x22,0x14,0x08,0x00, 0x02,0x01,0x51,0x09,0x06, // >  ?
    0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, // @  A
    0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22, // B  C
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, // D  E
    0x7F,0x09,0x09,0x01,0x01, 0x3E,0x41,0x41,0x51,0x32, // F  G
    0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, // H  I
    0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, // J  K
    0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x04,0x02,0x7F, // L  M
    0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E, // N  O
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, // P  Q
    0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31, // R  S
    0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, // T  U
    0x1F,0x20,0x40,0x20,0x1F, 0x7F,0x20,0x18,0x20,0x7F, // V  W
    0x63,0x14,0x08,0x14,0x63, 0x03,0x04,0x78,0x04,0x03, // X  Y
    0x61,0x51,0x49,0x45,0x43, 0x00,0x00,0x7F,0x41,0x41, // Z  [
    0x02,0x04,0x08,0x10,0x20, 0x41,0x41,0x7F,0x00,0x00, // \  ]
    0x04,0x02,0x01,0x02,0x04, 0x40,0x40,0x40,0x40,0x40, // ^  _
    0x00,0x01,0x02,0x04,0x00, 0x20,0x54,0x54,0x54,0x78, // `  a
    0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20, // b  c
    0x38,0x44,0x44,0x48,0x7F, 0x38,0x54,0x54,0x54,0x18, // d  e
    0x08,0x7E,0x09,0x01,0x02, 0x08,0x14,0x54,0x54,0x3C, // f  g
    0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, // h  i
    0x20,0x40,0x44,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00, // j  k
    0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x18,0x04,0x78, // l  m
    0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38, // n  o
    0x7C,0x14,0x14,0x14,0x08, 0x08,0x14,0x14,0x18,0x7C, // p  q
    0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20, // r  s
    0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, // t  u
    0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C, // v  w
    0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C, // x  y
    0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00, // z  {
    0x00,0x00,0x7F,0x00,0x00, 0x00,0x41,0x36,0x08,0x00, // |  }
    0x10,0x08,0x08,0x10,0x08,                             // ~
};

// ===================================================================
// GRAPHICS PRIMITIVES
// ===================================================================
static void _char(int x, int y, char c, int sz, bool inv) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 126) return;
    const uint8_t* g = &_font[(uc - 32) * 5];
    for (int col = 0; col < 5; col++) {
        uint8_t line = g[col];
        for (int row = 0; row < 7; row++) {
            if (line & (uint8_t)(1U << row)) {
                for (int dy = 0; dy < sz; dy++)
                    for (int dx = 0; dx < sz; dx++)
                        _pixel(x + col * sz + dx, y + row * sz + dy, !inv);
            }
        }
    }
}

static int _text(int x, int y, const char* s, int sz) {
    if (!s) return x;
    while (*s) { _char(x, y, *s, sz, false); x += 6 * sz; s++; }
    return x;
}

static int _text_inv(int x, int y, const char* s, int sz) {
    if (!s) return x;
    while (*s) { _char(x, y, *s, sz, true); x += 6 * sz; s++; }
    return x;
}

static void _hline(int x, int y, int w) {
    for (int i = 0; i < w; i++) _pixel(x + i, y, true);
}

static void _fill_rect(int x, int y, int w, int h, bool on) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            _pixel(x + i, y + j, on);
}

static void _rect(int x, int y, int w, int h) {
    _hline(x, y, w);
    _hline(x, y + h - 1, w);
    for (int j = 0; j < h; j++) { _pixel(x, y + j, true); _pixel(x + w - 1, y + j, true); }
}

static void _circle(int cx, int cy, int r, bool fill) {
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        if (fill) {
            _hline(cx - x, cy + y, 2 * x + 1);
            _hline(cx - x, cy - y, 2 * x + 1);
            _hline(cx - y, cy + x, 2 * y + 1);
            _hline(cx - y, cy - x, 2 * y + 1);
        } else {
            _pixel(cx + x, cy + y, true); _pixel(cx - x, cy + y, true);
            _pixel(cx + x, cy - y, true); _pixel(cx - x, cy - y, true);
            _pixel(cx + y, cy + x, true); _pixel(cx - y, cy + x, true);
            _pixel(cx + y, cy - x, true); _pixel(cx - y, cy - x, true);
        }
        y++;
        if (d <= 0) { d += 2 * y + 1; }
        else { x--; d += 2 * (y - x) + 1; }
    }
}

// ===================================================================
// UI HELPERS
// ===================================================================
static bool _should_refresh() {
    uint32_t now = millis();
    if ((uint32_t)(now - _last_refresh) < OLED_REFRESH_MS) return false;
    _last_refresh = now;
    _anim_frame++;
    return true;
}

static void _draw_header(const char* txt) {
    _fill_rect(0, 0, OLED_SCREEN_WIDTH, 12, true);
    if (txt) _text_inv(4, 2, txt, 1);
}

static void _draw_sep(int y) { _hline(0, y, OLED_SCREEN_WIDTH); }

static void _draw_footer(uint32_t start_ms) {
    const int y = 56;
    _draw_sep(y - 2);
    _circle(6, y + 3, 2, (_anim_frame % 2) != 0);

    uint32_t s = (millis() - start_ms) / 1000;
    char buf[16];
    if (s >= 3600) snprintf(buf, sizeof(buf), "UP:%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
    else           snprintf(buf, sizeof(buf), "UP:%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    _text(14, y, buf, 1);

    char sp[2] = { _spinner[_anim_frame % 4], 0 };
    _text(120, y, sp, 1);
}

static void _draw_big_num(int x, int y, uint32_t num) {
    char buf[16];
    if      (num >= 1000000UL) snprintf(buf, sizeof(buf), "%lu.%luM", (unsigned long)(num / 1000000UL), (unsigned long)((num % 1000000UL) / 100000UL));
    else if (num >= 100000UL)  snprintf(buf, sizeof(buf), "%luK", (unsigned long)(num / 1000UL));
    else                       snprintf(buf, sizeof(buf), "%lu", (unsigned long)num);
    _text(x, y, buf, 2);
}

// ===================================================================
// PUBLIC API
// ===================================================================
bool oled_init() {
    _i2c_init();
    delay(50);
    _ssd1306_init_hw();
    _force_redraw = true;
    memset(_prev_fb, 0xFF, sizeof(_prev_fb));
    _clear();
    _display();
    _initialized = true;
    return true;
}

bool oled_is_initialized() {
    return _initialized;
}

void oled_attack_start() {
    _attack_start = millis();
    _anim_frame = 0;
}

void oled_show_boot() {
    if (!_initialized) return;
    _clear();
    _rect(0, 0, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT);
    _rect(2, 2, OLED_SCREEN_WIDTH - 4, OLED_SCREEN_HEIGHT - 4);
    _text(40, 10, "BW16", 2);
    _text(41, 10, "BW16", 2);
    _text(16, 28, "DEAUTHER", 2);
    _text(17, 28, "DEAUTHER", 2);
    _hline(10, 46, 108);
    _text(18, 50, "v2.1 RTL8720DN", 1);
    _display();
}

void oled_show_scanning() {
    if (!_initialized) return;
    _clear();
    _draw_header("  WIFI SCANNER");
    _draw_sep(14);
    _text(16, 25, "Scanning WiFi...", 1);
    _rect(14, 38, 100, 10);
    _fill_rect(16, 40, 50, 6, true);
    _text(22, 52, "Please wait...", 1);
    _display();
}

void oled_show_idle(int ap24, int ap5g) {
    if (!_initialized) return;
    _clear();
    _draw_header(" DEAUTHER READY");
    _draw_sep(14);
    char buf[24];
    int total = ap24 + ap5g;
    if (total < 0) total = 0;
    _text(4, 21, "Networks:", 1);
    snprintf(buf, sizeof(buf), "%d", total);
    _text(64, 17, buf, 2);
    snprintf(buf, sizeof(buf), "2.4G:%d  5G:%d", ap24, ap5g);
    _text(4, 36, buf, 1);
    _draw_sep(47);
    _text(4, 50, "WebUI: 192.168.1.1", 1);
    _display();
}

void oled_show_deauth(int num_targets, uint32_t frames) {
    if (!_initialized || !_should_refresh()) return;
    _clear();
    _draw_header(" > DEAUTH ACTIVE");
    _draw_sep(14);
    char buf[24];
    snprintf(buf, sizeof(buf), "Targets: %d", num_targets);
    _text(4, 18, buf, 1);
    _draw_sep(28);
    _text(4, 31, "Frames:", 1);
    _draw_big_num(4, 39, frames);
    _draw_footer(_attack_start);
    _display();
}

void oled_show_interference(int active, int max_t) {
    if (!_initialized || !_should_refresh()) return;
    _clear();
    _draw_header(" > INTERFERENCE");
    _draw_sep(14);
    char buf[24];
    snprintf(buf, sizeof(buf), "Targets: %d/%d", active, max_t);
    _text(4, 18, buf, 1);
    _text(4, 30, "State: ", 1);
    if (active > 0) {
        _text(46, 30, "ACTIVE", 1);
        _circle(100, 34, 2, true);
        _circle(100, 34, 5, false);
        _circle(100, 34, 8, false);
    } else {
        _text(46, 30, "IDLE", 1);
    }
    _draw_footer(_attack_start);
    _display();
}

void oled_show_ble_spam(int type, uint32_t packets) {
    if (!_initialized || !_should_refresh()) return;
    _clear();
    _draw_header(" > BLE FLOOD");
    _draw_sep(14);
    _text(4, 18, "Type: ", 1);
    switch (type) {
        case 0:  _text(40, 18, "Apple Popup", 1); break;
        case 1:  _text(40, 18, "G. FastPair", 1); break;
        case 2:  _text(40, 18, "Win SwiftPair", 1); break;
        default: _text(40, 18, "Unknown", 1); break;
    }
    _draw_sep(28);
    _text(4, 31, "Packets:", 1);
    _draw_big_num(4, 39, packets);
    _draw_footer(_attack_start);
    _display();
}
    _text(4, 38, "Mode: Beacon Flood", 1);
    _draw_footer(_attack_start);
    _display();
}
