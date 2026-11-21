#ifndef MOCK_BLE_CONNECT_H
#define MOCK_BLE_CONNECT_H

#include "gap_le_types.h"
#include <cstdint>
#include <string>

class BLEConnect {
public:
    bool connect_should_succeed = true;
    int8_t fake_conn_id = 0;
    int8_t conn_id_after_polls = 0;
    int get_conn_id_polls = 0;
    std::string last_mac;
    T_GAP_REMOTE_ADDR_TYPE last_addr_type = GAP_REMOTE_ADDR_LE_PUBLIC;
    int disconnect_calls = 0;

    bool connect(char* bt_mac, T_GAP_REMOTE_ADDR_TYPE addr_type, uint32_t timeout_ms) {
        (void)timeout_ms;
        last_mac = bt_mac ? bt_mac : "";
        last_addr_type = addr_type;
        get_conn_id_polls = 0;
        return connect_should_succeed;
    }

    int8_t getConnId(char* bt_mac, uint8_t addr_type) {
        (void)bt_mac;
        (void)addr_type;
        get_conn_id_polls++;
        if (get_conn_id_polls <= conn_id_after_polls) return -1;
        return fake_conn_id;
    }

#endif // MOCK_BLE_CONNECT_H
