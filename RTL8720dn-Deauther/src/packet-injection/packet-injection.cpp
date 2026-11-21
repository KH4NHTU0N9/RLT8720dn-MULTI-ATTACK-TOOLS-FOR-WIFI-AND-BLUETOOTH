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
