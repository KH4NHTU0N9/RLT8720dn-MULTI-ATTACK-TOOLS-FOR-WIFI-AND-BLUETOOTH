#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include <stdint.h>

#define DEBUG
#define DEBUG_BAUD 115200

#ifdef DEBUG
  #define DEBUG_SER_INIT() \
    do { Serial.begin(DEBUG_BAUD); } while (0)
  #define DEBUG_SER_PRINT(...) \
    do { Serial.print(__VA_ARGS__); } while (0)
  #define DEBUG_SER_PRINTLN(...) \
    do { Serial.println(__VA_ARGS__); } while (0)
#else
  #define DEBUG_SER_INIT() \
    do {} while (0)
  #define DEBUG_SER_PRINT(...) \
    do {} while (0)
  #define DEBUG_SER_PRINTLN(...) \
    do {} while (0)
