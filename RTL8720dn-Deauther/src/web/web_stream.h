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
#endif // WEB_STREAM_H
