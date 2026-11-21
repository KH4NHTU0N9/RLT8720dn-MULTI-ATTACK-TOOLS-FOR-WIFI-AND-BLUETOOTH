#ifndef WEB_ROUTER_H
#define WEB_ROUTER_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <stddef.h>
#include <utility>
#include <vector>
#include "../core/app_state.h"
#include "web_stream.h"

class WiFiServer;

// Parse an application/x-www-form-urlencoded body slice into decoded key-value pairs.
void webParsePostBody(const char* body, size_t body_len,
                      std::vector<std::pair<String, String>>& out_params);

// Parameter & validation helpers (shared cleanly with host-side unit tests).
int webClampFramesPerDeauth(const String& raw, int fallback);
uint16_t webClampDeauthReason(const String& raw, uint16_t fallback);
int webClampBeaconsPerSsid(const String& raw, int fallback);
int webClampRandomSsidCount(const String& raw);
int webClampBleSpamType(const String& raw);

#endif // WEB_ROUTER_H
