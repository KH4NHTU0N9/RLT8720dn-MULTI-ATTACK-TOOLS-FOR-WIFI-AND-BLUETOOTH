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

// Route a pre-read HTTP request buffer against an abstract output sink.
// Returns true if server/WiFi were stopped for an attack and caller must skip trailing client.stop().
bool webRouteRawRequest(const char* raw_req, size_t req_len,
                        IWebOutput& output,
                        bool (*post_scan_cb)(void* ctx),
                        void (*prepare_attack_cb)(void* ctx),
                        void* cb_ctx);

// Poll WiFiServer for an incoming client, read bounded HTTP request, and dispatch.
void webServeClient(WiFiServer& server);

#endif // WEB_ROUTER_H
