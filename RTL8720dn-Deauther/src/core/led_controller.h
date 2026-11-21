#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <stdint.h>

// Initialize RGB LED GPIO pins and set initial idle state.
void ledInit();

// Enable or disable all RGB LED activity globally.
void ledSetEnabled(bool enabled);
bool ledIsEnabled();

// Set explicit RGB channels (respects ledIsEnabled()).
#endif // LED_CONTROLLER_H
