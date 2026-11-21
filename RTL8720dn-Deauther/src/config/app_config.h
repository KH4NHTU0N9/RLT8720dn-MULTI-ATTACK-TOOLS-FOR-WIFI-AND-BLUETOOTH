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
#endif // APP_CONFIG_H
