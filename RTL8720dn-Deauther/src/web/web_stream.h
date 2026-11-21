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

#endif // WEB_STREAM_H
