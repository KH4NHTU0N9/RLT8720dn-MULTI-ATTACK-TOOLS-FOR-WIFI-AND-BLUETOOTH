#ifndef MOCK_WIFI_STRUCTURES_H
#define MOCK_WIFI_STRUCTURES_H

#include <cstdint>

typedef int rtw_result_t;
#define RTW_SUCCESS 0
#define RTW_ERROR   (-1)

#define RTW_MODE_NONE   0
#define RTW_MODE_STA    1
#define RTW_MODE_AP     2
#define RTW_MODE_STA_AP 3

typedef int rtw_rcr_level_t;
#define RTW_PROMISC_DISABLE   0
#define RTW_PROMISC_ENABLE    1
#define RTW_PROMISC_ENABLE_1  2
#define RTW_PROMISC_ENABLE_2  3

typedef struct {
    uint8_t len;
    uint8_t val[33];
} rtw_ssid_t;

typedef struct {
    uint8_t octet[6];
} rtw_mac_t;

typedef struct {
    rtw_ssid_t SSID;
    rtw_mac_t  BSSID;
    int16_t    signal_strength;
    uint32_t   channel;
} rtw_scan_result_t;

typedef struct {
    int               scan_complete;
    rtw_scan_result_t ap_details;
} rtw_scan_handler_result_t;

typedef void (*promisc_callback_t)(unsigned char* buf, unsigned int len, void* userdata);
typedef rtw_result_t (*scan_result_handler_t)(rtw_scan_handler_result_t* malloced_scan_result);

#endif // MOCK_WIFI_STRUCTURES_H
