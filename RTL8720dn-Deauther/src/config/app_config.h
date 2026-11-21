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

// Timing constants (milliseconds)
#define BOOT_SPLASH_MS            1500
#define WIFI_SCAN_TIMEOUT_MS      7500
#define HTTP_REQ_TIMEOUT_MS       1000
#define TCP_FLUSH_DELAY_MS        50
#define BLE_SCAN_DURATION_MS      3000
#define BLE_CONNECT_WAIT_MS       2200
#define BLE_DISCOVERY_TIMEOUT_MS  5000
#define BLE_ADV_STOP_SETTLE_MS    10
#define BLE_ADV_DWELL_MS          200
#define BAND_SWITCH_DELAY_MS      15
#define SAME_BAND_DELAY_MS        2
#define IDLE_LOOP_DELAY_MS        2

// HTTP Streaming buffer size (fits comfortably within LwIP 1460-byte TCP MSS)
#define WEB_STREAM_CHUNK_SIZE     1024

// Host-side unit test fallback pin definitions
#ifndef LED_R
#define LED_R 12
#endif
#ifndef LED_G
#define LED_G 10
#endif
#ifndef LED_B
#define LED_B 11
#endif

#endif // APP_CONFIG_H
