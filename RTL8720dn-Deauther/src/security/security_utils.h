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

#endif // SECURITY_UTILS_H
