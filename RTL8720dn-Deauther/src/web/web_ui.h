#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <stddef.h>
#include <vector>
#include "../core/app_state.h"
#include "web_stream.h"

// Render a single Wi-Fi AP row into the active WebStream.
void webRenderScanRow(WebStream& out, const WiFiScanResult& ap, size_t idx, const char* group_cls);

// Render the complete single-page Web UI (HTTP 200 headers + HTML/CSS/JS) into output.
void webRenderDashboard(IWebOutput& output,
                        const AppState& app,
                        const std::vector<WiFiScanResult>& scan_results,
                        const std::vector<BLEAuditResult>& ble_results,
                        bool led_enabled,
                        size_t free_heap_bytes);

// Render an HTTP 404 or status error response into output.
void webRenderPlainError(IWebOutput& output, int code, const char* body);

#endif // WEB_UI_H
