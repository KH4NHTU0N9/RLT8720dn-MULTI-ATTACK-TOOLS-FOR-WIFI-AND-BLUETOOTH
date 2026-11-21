#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stddef.h>
#include <stdint.h>

// Security note: Change default SoftAP passphrase before field deployment (CWE-798).
#define DEFAULT_AP_SSID          "RTL8720dn-Deauther"
#define DEFAULT_AP_PASS          "0123456789"
#define DEFAULT_AP_CHANNEL       1
#define DEFAULT_WEBUI_IP         "192.168.1.1"

// Capacity bounds (tuned for 250 KB FreeRTOS heap on RTL8720DN)
#define MAX_SCAN_RESULTS         64
#define MAX_BLE_RESULTS          20
#define MAX_BLE_WRITE_BYTES      512
#define MAX_HTTP_REQ_SIZE        6144
#define MAX_SSID_BYTES           32
#define MAX_SPAM_NETWORKS        64
#define MAX_INTERFERENCE_TARGETS 4

// Attack parameter defaults & bounds
#define DEFAULT_FRAMES_PER_DEAUTH 100
#define MIN_FRAMES_PER_DEAUTH     1
#define MAX_FRAMES_PER_DEAUTH     5000
#define DEFAULT_DEAUTH_REASON     2
#define DEFAULT_BEACONS_PER_SSID  3
#define MAX_BEACONS_PER_SSID      20

#endif // APP_CONFIG_H
