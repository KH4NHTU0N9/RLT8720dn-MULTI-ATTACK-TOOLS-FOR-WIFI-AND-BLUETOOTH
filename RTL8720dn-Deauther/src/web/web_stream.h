#ifndef WEB_STREAM_H
#define WEB_STREAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../config/app_config.h"

// Abstract byte sink so WebStream and WebUI compile identically on RTL8720DN
// (wrapping WiFiClient) and on macOS/Linux host unit tests (wrapping std::string).
class IWebOutput {
public:
    virtual ~IWebOutput() = default;
    virtual size_t writeBytes(const uint8_t* data, size_t len) = 0;
};

// Buffered HTTP writer that batches small HTML fragments into <=1024-byte chunks
// before flushing via IWebOutput::writeBytes().
class WebStream {
public:
    explicit WebStream(IWebOutput& output)
        : _output(output), _len(0), _aborted(false), _total_bytes(0) {}

    ~WebStream() {
        flush();
    }

    bool aborted() const { return _aborted; }
    size_t totalBytes() const { return _total_bytes; }

    void writeBytes(const char* data, size_t len) {
        if (_aborted || !data || len == 0) return;

        size_t pos = 0;
        while (pos < len) {
            size_t avail = sizeof(_buf) - _len;
            if (avail == 0) {
                if (!flush()) return;
                avail = sizeof(_buf);
            }
            size_t take = len - pos;
            if (take > avail) take = avail;
            memcpy(&_buf[_len], data + pos, take);
            _len += take;
            pos += take;
            if (_len == sizeof(_buf)) {
                if (!flush()) return;
            }
        }
    }

    void writeStr(const char* cstr) {
        if (!cstr) return;
        writeBytes(cstr, strlen(cstr));
    }

    void writeChar(char c) {
        writeBytes(&c, 1);
    }

    void writeInt(long val) {
        char tmp[24];
        int n = snprintf(tmp, sizeof(tmp), "%ld", val);
        if (n > 0) writeBytes(tmp, (size_t)n);
    }

    void writeUInt(unsigned long val) {
        char tmp[24];
        int n = snprintf(tmp, sizeof(tmp), "%lu", val);
        if (n > 0) writeBytes(tmp, (size_t)n);
    }

    // Stream HTML-escaped text directly into the chunk buffer with zero heap allocations.
    void writeHtmlEscaped(const char* raw) {
        if (!raw || _aborted) return;
        const char* seg_start = raw;
        const char* p = raw;
        while (*p) {
            const char* esc = nullptr;
            switch (*p) {
                case '&':  esc = "&amp;";  break;
                case '<':  esc = "&lt;";   break;
                case '>':  esc = "&gt;";   break;
                case '"':  esc = "&quot;"; break;
                case '\'': esc = "&#39;";  break;
                default:   break;
            }
            if (esc) {
                if (p > seg_start) {
                    writeBytes(seg_start, (size_t)(p - seg_start));
#endif // WEB_STREAM_H
