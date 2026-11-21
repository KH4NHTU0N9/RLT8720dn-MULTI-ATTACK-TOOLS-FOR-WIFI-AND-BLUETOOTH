#ifndef SECURITY_UTILS_H
#define SECURITY_UTILS_H

#include <Arduino.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../config/app_config.h"

// Escape HTML/JS special characters to prevent Stored/Reflected XSS in Web UI.
static inline String htmlEscape(const String& input) {
    String out;
    out.reserve(input.length() + 16);
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

// Convert a single ASCII hex digit to 0..15, or -1 if invalid.
static inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decode application/x-www-form-urlencoded values while blocking null-byte injection.
static inline String urlDecode(const String& src) {
    String out;
    out.reserve(src.length());
    unsigned int len = src.length();
    for (unsigned int i = 0; i < len; i++) {
        char c = src[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && (i + 2) < len) {
            int hi = hexNibble(src[i + 1]);
            int lo = hexNibble(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                char decoded = (char)((hi << 4) | lo);
                if (decoded != '\0') {
                    out += decoded;
                }
                i += 2;
                continue;
            }
            out += c;
        } else {
            out += c;
        }
    }
    return out;
}

// Safely validate and decode a hex string (supports surrounding spaces / optional 0x prefix
// and spaces between bytes) into a caller-provided fixed buffer.
static inline bool decodeHexPayload(const String& hex_data, uint8_t* out_buf, size_t max_out_len, size_t* out_len) {
    if (!out_buf || !out_len || max_out_len == 0) return false;
    *out_len = 0;

    const char* raw = hex_data.c_str();
    size_t raw_len = hex_data.length();
    size_t start = 0;
    while (start < raw_len && isspace((unsigned char)raw[start])) start++;
    while (raw_len > start && isspace((unsigned char)raw[raw_len - 1])) raw_len--;

    if ((raw_len - start) >= 2 && raw[start] == '0' && (raw[start + 1] == 'x' || raw[start + 1] == 'X')) {
        start += 2;
    }

    int high_nibble = -1;
    size_t byte_count = 0;
    for (size_t i = start; i < raw_len; i++) {
        char c = raw[i];
        if (c == ' ' || c == '\t') {
            if (high_nibble >= 0) return false; // Disallow splitting nibbles within a byte
            continue;
        }
        int nib = hexNibble(c);
        if (nib < 0) {
            *out_len = 0;
            return false;
        }
        if (high_nibble < 0) {
            high_nibble = nib;
        } else {
            if (byte_count >= max_out_len) {
                *out_len = 0;
                return false;
            }
            out_buf[byte_count++] = (uint8_t)((high_nibble << 4) | nib);
            high_nibble = -1;
        }
    }

    if (high_nibble >= 0 || byte_count == 0) {
        *out_len = 0;
        return false;
    }

    *out_len = byte_count;
    return true;
}

// Safely parse Complete/Shortened Local Name from BLE advertising payload with strict bounds checks.
static inline String parseBleDeviceName(const uint8_t* p_adv, size_t len) {
    if (!p_adv || len == 0) return String("[Unknown]");

    size_t offset = 0;
    while (offset < len) {
        uint8_t ad_len = p_adv[offset];
        if (ad_len == 0) break;
        if (offset + 1 >= len) break;
        if (offset + 1 + (size_t)ad_len > len) break;

        uint8_t ad_type = p_adv[offset + 1];
        if ((ad_type == 0x09 || ad_type == 0x08) && ad_len >= 2) {
            char name_buf[32];
            size_t name_len = (size_t)(ad_len - 1);
            if (name_len > sizeof(name_buf) - 1) {
                name_len = sizeof(name_buf) - 1;
            }
            size_t out_pos = 0;
            for (size_t j = 0; j < name_len; j++) {
                uint8_t ch = p_adv[offset + 2 + j];
                if (ch >= 0x20 && ch != 0x7F) {
                    name_buf[out_pos++] = (char)ch;
                }
            }
            name_buf[out_pos] = '\0';
            if (out_pos > 0) {
                return String(name_buf);
            }
        }
        offset += (size_t)ad_len + 1;
    }
    return String("[Unknown]");
}

// Safely copy a raw 802.11 SSID buffer without mutating the source SDK struct,
// filtering non-printable control characters.
static inline String sanitizeSsid(const uint8_t* raw_ssid, size_t raw_len) {
    if (!raw_ssid || raw_len == 0) return String("");
    char ssid_buf[MAX_SSID_BYTES + 1];
    size_t copy_len = raw_len;
    if (copy_len > MAX_SSID_BYTES) copy_len = MAX_SSID_BYTES;
    size_t out_pos = 0;
    for (size_t i = 0; i < copy_len; i++) {
        uint8_t ch = raw_ssid[i];
        if (ch >= 0x20 && ch != 0x7F) {
            ssid_buf[out_pos++] = (char)ch;
        }
    }
    ssid_buf[out_pos] = '\0';
    return String(ssid_buf);
}

// Format a 6-byte MAC address as uppercase colon-separated hex.
static inline void formatMacAddress(const uint8_t mac[6], char* out, size_t out_size) {
    if (!out || out_size < 18) return;
    if (!mac) {
        snprintf(out, out_size, "00:00:00:00:00:00");
        return;
    }
    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Strict integer parser (avoids String::toInt() silently returning 0 on "" or "abc").
static inline bool parseStrictInt(const String& src, long* out_val) {
    if (!out_val) return false;
    const char* s = src.c_str();
    if (!s) return false;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return false;

    char* endptr = nullptr;
    long val = strtol(s, &endptr, 10);
    if (endptr == s) return false;
    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0') return false;

    *out_val = val;
    return true;
}

static inline bool parseStrictIntRange(const String& src, long min_val, long max_val, long* out_val) {
    long tmp = 0;
    if (!parseStrictInt(src, &tmp)) return false;
    if (tmp < min_val || tmp > max_val) return false;
    *out_val = tmp;
    return true;
}

// Channel band helpers (consistent between scanner counts, UI rendering, and attack runners).
static inline bool is24GhzChannel(int ch) {
    return ch >= 1 && ch <= 14;
}

static inline bool is5GhzChannel(int ch) {
    return ch > 14;
}

// Clamp RSSI signal meter width percentage to valid [0, 100] CSS range.
static inline int clampRssiPercent(int rssi) {
    int width = rssi + 120;
    if (width < 0) return 0;
    if (width > 100) return 100;
    return width;
}

// Build RFC 9112 compliant HTTP response headers with CRLF line endings.
static inline String buildHttpResponseHeaders(int code, const String& content_type) {
    const char* reason = "OK";
    switch (code) {
        case 200: reason = "OK"; break;
        case 204: reason = "No Content"; break;
        case 400: reason = "Bad Request"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 413: reason = "Payload Too Large"; break;
        case 500: reason = "Internal Server Error"; break;
        default:  reason = "OK"; break;
    }
    String response;
    response.reserve(192);
    response += "HTTP/1.1 ";
    response += String(code);
    response += " ";
    response += reason;
    response += "\r\nContent-Type: ";
    response += content_type;
    response += "\r\nCache-Control: no-store\r\n";
    response += "X-Content-Type-Options: nosniff\r\n";
    response += "X-Frame-Options: DENY\r\n";
    response += "Connection: close\r\n\r\n";
    return response;
}

// Build RFC 9110/9112 compliant HTTP 303 See Other redirect (terminates POST->GET cleanly).
static inline String buildHttpRedirectHeaders(const String& url) {
    String response;
    response.reserve(96 + url.length());
    response += "HTTP/1.1 303 See Other\r\nLocation: ";
    response += url;
    response += "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    return response;
}

// Parse HTTP request line into method and path (strips optional ?query string).
// Returns false on empty, missing method/path, or malformed request lines.
static inline bool parseHttpRequestLine(const char* req, size_t req_len,
                                        char* method_out, size_t method_cap,
                                        char* path_out, size_t path_cap) {
    if (!req || req_len == 0 || !method_out || method_cap < 2 || !path_out || path_cap < 2) {
        return false;
    }
    // Find first space separating method and target
    size_t sp1 = 0;
    while (sp1 < req_len && req[sp1] != ' ' && req[sp1] != '\r' && req[sp1] != '\n') {
        sp1++;
    }
    if (sp1 == 0 || sp1 >= req_len || req[sp1] != ' ' || sp1 >= method_cap) {
        return false;
    }
    memcpy(method_out, req, sp1);
    method_out[sp1] = '\0';

    size_t p_start = sp1 + 1;
    size_t p_end = p_start;
    while (p_end < req_len && req[p_end] != ' ' && req[p_end] != '\r' && req[p_end] != '\n') {
        p_end++;
    }
    if (p_end <= p_start) return false;

    // Strip optional query string '?...'
    size_t q_end = p_start;
    while (q_end < p_end && req[q_end] != '?') {
        q_end++;
    }
    size_t path_len = q_end - p_start;
    if (path_len == 0 || path_len >= path_cap) {
        return false;
    }
    memcpy(path_out, req + p_start, path_len);
    path_out[path_len] = '\0';
    return true;
}

// Case-insensitive Content-Length extraction anchored at header line start.
// Returns:  1 = valid Content-Length parsed into *out_len
//           0 = no Content-Length header found (*out_len = 0)
//          -1 = malformed/negative/overflow Content-Length header (reject 400/413)
static inline int extractHttpContentLength(const char* req, size_t header_end, long* out_len) {
    if (!req || !out_len) return -1;
    *out_len = 0;

    static const char kHeaderName[] = "content-length:";
    const size_t kNameLen = sizeof(kHeaderName) - 1;

    size_t pos = 0;
    // Skip request line
    while (pos < header_end && req[pos] != '\n') pos++;
    if (pos < header_end) pos++;

    while (pos < header_end) {
        size_t line_start = pos;
        size_t line_end = line_start;
        while (line_end < header_end && req[line_end] != '\r' && req[line_end] != '\n') {
            line_end++;
        }

        if ((line_end - line_start) >= kNameLen) {
            bool match = true;
            for (size_t i = 0; i < kNameLen; i++) {
                if (tolower((unsigned char)req[line_start + i]) != (unsigned char)kHeaderName[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                size_t v_start = line_start + kNameLen;
                while (v_start < line_end && (req[v_start] == ' ' || req[v_start] == '\t')) {
                    v_start++;
                }
                size_t v_end = line_end;
                while (v_end > v_start && (req[v_end - 1] == ' ' || req[v_end - 1] == '\t')) {
                    v_end--;
                }
#endif // SECURITY_UTILS_H
