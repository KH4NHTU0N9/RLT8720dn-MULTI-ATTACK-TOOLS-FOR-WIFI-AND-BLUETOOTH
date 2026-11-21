#ifndef PACKET_INJECTION_H
#define PACKET_INJECTION_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// IEEE 802.11 Maximum valid NAV duration (bit 15 = 0; 0xFFFF is Reserved per 802.11-2020 9.2.4.2)
#define IEEE80211_MAX_NAV_DURATION 0x7FFF

struct __attribute__((packed)) DeauthFrame {
  uint16_t frame_control = 0xC0;
  uint16_t duration = 0x0000;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x06;
};

