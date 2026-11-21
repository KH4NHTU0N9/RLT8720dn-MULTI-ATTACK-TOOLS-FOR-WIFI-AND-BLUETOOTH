#ifndef MOCK_GAP_CALLBACK_LE_H
#define MOCK_GAP_CALLBACK_LE_H

#include "gap_le_types.h"
#include <cstdint>

typedef struct {
    uint8_t bd_addr[6];
    uint8_t remote_addr_type;
    int8_t  rssi;
    uint8_t data[31];
    uint8_t data_len;
} T_LE_SCAN_INFO;

typedef struct {
    T_LE_SCAN_INFO* p_le_scan_info;
} T_LE_CB_DATA;

#endif // MOCK_GAP_CALLBACK_LE_H
