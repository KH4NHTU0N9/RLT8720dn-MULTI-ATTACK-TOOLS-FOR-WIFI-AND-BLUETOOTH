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

// Non-QoS Null Data frame (24 bytes; no QoS Control field on subtype 4 = 0x48)
struct __attribute__((packed)) NullFrame {
  uint16_t frame_control = 0x48;
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
};

struct __attribute__((packed)) ProbeRequest {
  uint16_t frame_control = 0x40;
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
};

struct __attribute__((packed)) CSAFrame {
  uint16_t frame_control = 0xD0; // Action frame
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint8_t category = 0;        // Spectrum Management
  uint8_t action = 4;          // Channel Switch Announcement
  uint8_t element_id = 37;
  uint8_t ie_length = 3;
  uint8_t switch_mode = 1;
  uint8_t new_channel = 1;
  uint8_t switch_count = 1;
};

extern uint8_t* rltk_wlan_info;
extern "C" unsigned char rltk_wlan_running(unsigned char idx);
extern "C" void* alloc_mgtxmitframe(void* ptr);
extern "C" void update_mgntframe_attrib(void* ptr, void* frame_control);
extern "C" int dump_mgntframe(void* ptr, void* frame_control);

bool wifi_tx_raw_frame(void* frame, size_t length);
bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x06, void* bssid = nullptr);
bool wifi_tx_deauth_nav(void* src_mac, void* dst_mac, uint16_t reason = 0x06, void* bssid = nullptr);
bool wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason = 0x06, void* bssid = nullptr);
bool wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid);
bool wifi_tx_auth_frame(void* src_mac, void* dst_mac);
bool wifi_tx_assoc_frame(void* src_mac, void* dst_mac);
bool wifi_tx_null_frame(void* src_mac, void* dst_mac);
bool wifi_tx_probe_frame(void* src_mac, void* dst_mac);
