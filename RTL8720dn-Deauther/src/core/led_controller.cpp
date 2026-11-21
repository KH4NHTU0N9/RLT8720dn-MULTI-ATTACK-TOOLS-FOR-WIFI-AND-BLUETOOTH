#include "led_controller.h"
#include "../config/app_config.h"

static bool g_led_initialized = false;
static bool g_led_enabled = true;
static bool g_r_state = false;
static bool g_g_state = false;
static bool g_b_state = false;

static void applyPins() {
    if (!g_led_initialized) return;
    if (!g_led_enabled) {
        digitalWrite(LED_R, LOW);
        digitalWrite(LED_G, LOW);
        digitalWrite(LED_B, LOW);
        return;
    }
    digitalWrite(LED_R, g_r_state ? HIGH : LOW);
    digitalWrite(LED_G, g_g_state ? HIGH : LOW);
    digitalWrite(LED_B, g_b_state ? HIGH : LOW);
}
