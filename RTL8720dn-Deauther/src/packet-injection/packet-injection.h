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

struct __attribute__((packed)) DisassocFrame {
  uint16_t frame_control = 0xA0;
  uint16_t duration = 0x0000;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x06;
};

struct __attribute__((packed)) BeaconFrame {
  uint16_t frame_control = 0x80;
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint64_t timestamp = 0;
  uint16_t beacon_interval = 0x64;
  uint16_t ap_capabilities = 0x0411;
  const uint8_t ssid_tag = 0;
  uint8_t ssid_length = 0;
  uint8_t ssid[255];
};

struct __attribute__((packed)) AuthFrame {
  uint16_t frame_control = 0xB0;
  uint16_t duration = IEEE80211_MAX_NAV_DURATION;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t auth_algorithm = 0;
  uint16_t auth_sequence = 1;
  uint16_t status_code = 0;
};

struct __attribute__((packed)) AssocFrame {
  uint16_t frame_control = 0x00;
  uint16_t duration = IEEE80211_MAX_NAV_DURATION;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t cap_info = 0x0411;
  uint16_t listen_interval = 0x0001;
};
