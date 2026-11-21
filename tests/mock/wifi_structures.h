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

#endif // MOCK_WIFI_STRUCTURES_H
