#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include <stdint.h>

// ===================================================================
// OLED SSD1306 128x64 I2C Display Module for BW16 (RTL8720DN)
// Wiring: PA25 (Arduino Pin 7) -> SCL, PA26 (Arduino Pin 8) -> SDA
// Software I2C (bypasses Wire pin-overwrite bug; ignores ACK when no OLED attached)
// ===================================================================

#define OLED_SCREEN_WIDTH   128
#define OLED_SCREEN_HEIGHT  64
#define OLED_PAGE_COUNT     8
#define OLED_I2C_ADDR       0x3C
#define OLED_REFRESH_MS     200   // Throttle updates to avoid attack lag

// Initialize GPIO & SSD1306 controller. Returns true once driver is ready.
bool oled_init();
bool oled_is_initialized();

// Reset attack uptime counter when an attack starts.
void oled_attack_start();

// Screen rendering functions
void oled_show_boot();
void oled_show_scanning();
void oled_show_idle(int ap_count_24, int ap_count_5g);
void oled_show_deauth(int num_targets, uint32_t frames);
void oled_show_interference(int active_targets, int max_targets);
void oled_show_ble_spam(int type, uint32_t packets);
void oled_show_spammer(int ssid_count);

#endif // OLED_DISPLAY_H
