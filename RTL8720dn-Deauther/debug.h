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
#endif

// Print a 6-byte MAC address without allocating a heap String.
static inline void debugPrintMac(const uint8_t mac[6]) {
#ifdef DEBUG
  if (!mac) return;
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(buf);
#else
  (void)mac;
#endif
}

#endif // DEBUG_H
