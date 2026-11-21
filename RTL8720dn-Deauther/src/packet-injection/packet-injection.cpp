#include "packet-injection.h"
#include <string.h>

static void _fill_mac_addrs(uint8_t* dst_field, uint8_t* src_field, uint8_t* ap_field,
                            const void* src_mac, const void* dst_mac, const void* bssid) {
  memcpy(dst_field, dst_mac, 6);
  memcpy(src_field, src_mac, 6);
  memcpy(ap_field, bssid ? bssid : src_mac, 6);
}

static uint8_t _operating_class_for_channel(uint8_t ch) {
  if (ch <= 14) return 81;           // 2.4 GHz ISM (20 MHz)
  if (ch >= 36 && ch <= 48) return 115; // 5 GHz UNII-1
  if (ch >= 52 && ch <= 64) return 118; // 5 GHz UNII-2A
  if (ch >= 100 && ch <= 144) return 121; // 5 GHz UNII-2C
  return 124;                         // 5 GHz UNII-3 (149-165)
}

#ifdef UNIT_TEST
extern "C" bool unit_test_capture_raw_frame(const void* frame, size_t length);
#endif

bool wifi_tx_raw_frame(void* frame, size_t length) {
  // Minimum 802.11 MAC header is 24 bytes (update_mgntframe_attrib sets hdrlen = 24)
  if (frame == nullptr || length < 24 || length > 1536) {
    return false;
  }
  if (!rltk_wlan_running(0) || rltk_wlan_info == nullptr) {
    return false;
  }
#ifdef UNIT_TEST
  return unit_test_capture_raw_frame(frame, length);
#else
  uint32_t **wlan_ptr = (uint32_t **)(rltk_wlan_info + 0x10);
  if (*wlan_ptr == nullptr) {
    return false;
  }
  uint8_t *ptr = (uint8_t *)(**wlan_ptr);
  if (ptr == nullptr) {
    return false;
  }
  uint8_t *frame_control = (uint8_t *)alloc_mgtxmitframe(ptr + 0xa80);
  if (frame_control == nullptr) {
    return false;
  }

  update_mgntframe_attrib(ptr, frame_control + 8);
  uint8_t *tx_buf = (uint8_t *)*(uint32_t *)(frame_control + 0x80);
  if (tx_buf == nullptr) {
    return false;
  }
  memset((void *)tx_buf, 0, 0x68);
  uint8_t *frame_data = tx_buf + 0x28;
  memcpy(frame_data, frame, length);
  *(uint32_t *)(frame_control + 0x14) = (uint32_t)length;
  *(uint32_t *)(frame_control + 0x18) = (uint32_t)length;
  dump_mgntframe(ptr, frame_control);
  return true;
#endif
}

bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, uint16_t reason, void* bssid) {
  if (src_mac == nullptr || dst_mac == nullptr) return false;
  DeauthFrame frame;
  _fill_mac_addrs(frame.destination, frame.source, frame.access_point, src_mac, dst_mac, bssid);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

bool wifi_tx_deauth_nav(void* src_mac, void* dst_mac, uint16_t reason, void* bssid) {
  if (src_mac == nullptr || dst_mac == nullptr) return false;
  DeauthFrame frame;
  frame.duration = IEEE80211_MAX_NAV_DURATION;
  _fill_mac_addrs(frame.destination, frame.source, frame.access_point, src_mac, dst_mac, bssid);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

bool wifi_tx_disassoc_frame(void* src_mac, void* dst_mac, uint16_t reason, void* bssid) {
  if (src_mac == nullptr || dst_mac == nullptr) return false;
  DisassocFrame frame;
  _fill_mac_addrs(frame.destination, frame.source, frame.access_point, src_mac, dst_mac, bssid);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DisassocFrame));
}

bool wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid) {
  if (src_mac == nullptr || dst_mac == nullptr || ssid == nullptr) return false;

  uint8_t frame[128];
  memset(frame, 0, 36);
  frame[0] = 0x80;
  _fill_mac_addrs(&frame[4], &frame[10], &frame[16], src_mac, dst_mac, src_mac);

