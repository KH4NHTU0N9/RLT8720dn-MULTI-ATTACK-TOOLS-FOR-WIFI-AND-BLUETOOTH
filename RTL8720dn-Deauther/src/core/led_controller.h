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
void ledSetColor(bool r, bool g, bool b);
void ledSetRed(bool on);
void ledSetGreen(bool on);
void ledSetBlue(bool on);
void ledToggleGreen();
void ledToggleBlue();
void ledOff();

// Semantic state presets
void ledShowIdle();
void ledShowHttpBusy();
void ledShowAttackOff();

#endif // LED_CONTROLLER_H
