#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define LOW          0x0
#define HIGH         0x1
#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

class String {
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(int val) : _s(std::to_string(val)) {}
    String(unsigned int val) : _s(std::to_string(val)) {}
    String(long val) : _s(std::to_string(val)) {}
    String(unsigned long val) : _s(std::to_string(val)) {}

    void reserve(unsigned int size) { _s.reserve(size); }
    unsigned int length() const { return static_cast<unsigned int>(_s.size()); }
    const char* c_str() const { return _s.c_str(); }
    const std::string& std_str() const { return _s; }

    char operator[](unsigned int index) const {
        return (index < _s.size()) ? _s[index] : '\0';
    }

    String& operator+=(const String& rhs) {
        _s += rhs._s;
        return *this;
    }

    String& operator+=(const char* rhs) {
        if (rhs) _s += rhs;
        return *this;
    }

    String& operator+=(char c) {
        _s += c;
        return *this;
    }

    bool operator==(const String& rhs) const { return _s == rhs._s; }
    bool operator==(const char* rhs) const { return _s == (rhs ? rhs : ""); }
    bool operator!=(const String& rhs) const { return !(*this == rhs); }
    bool operator!=(const char* rhs) const { return !(*this == rhs); }

    int indexOf(const char* needle, unsigned int fromIndex = 0) const {
        if (!needle || fromIndex > _s.size()) return -1;
        size_t pos = _s.find(needle, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    String substring(unsigned int left) const {
        if (left >= _s.size()) return String("");
        return String(_s.substr(left));
    }

    String substring(unsigned int left, unsigned int right) const {
        if (left > right) std::swap(left, right);
        if (left >= _s.size()) return String("");
        return String(_s.substr(left, right - left));
    }

    void trim() {
        size_t start = 0;
        while (start < _s.size() && std::isspace(static_cast<unsigned char>(_s[start]))) {
            start++;
        }
        size_t end = _s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(_s[end - 1]))) {
            end--;
        }
        _s = _s.substr(start, end - start);
    }

    long toInt() const {
        return std::strtol(_s.c_str(), nullptr, 10);
    }

    friend String operator+(const String& lhs, const String& rhs) {
        return String(lhs._s + rhs._s);
    }

    friend String operator+(const char* lhs, const String& rhs) {
        return String(std::string(lhs ? lhs : "") + rhs._s);
    }

    friend String operator+(const String& lhs, const char* rhs) {
        return String(lhs._s + std::string(rhs ? rhs : ""));
    }

private:
    std::string _s;
};

struct MockArduinoState {
    unsigned long clock_ms = 0;
    unsigned long long clock_us = 0;
    uint32_t prng_state = 0x12345678u;

    bool pin_initialized[64] = {false};
    uint8_t pin_mode[64] = {0};
    uint8_t pin_level[64] = {0};
    uint32_t digital_write_calls = 0;
    uint32_t pin_mode_calls = 0;
    uint32_t write_before_pinmode_violations = 0;

    void (*on_gpio_event)(uint8_t pin, uint8_t mode, uint8_t level, void* ctx) = nullptr;
    void* on_gpio_ctx = nullptr;

    void reset() {
        clock_ms = 0;
        clock_us = 0;
        prng_state = 0x12345678u;
        std::memset(pin_initialized, 0, sizeof(pin_initialized));
        std::memset(pin_mode, 0, sizeof(pin_mode));
        std::memset(pin_level, 0, sizeof(pin_level));
        digital_write_calls = 0;
        pin_mode_calls = 0;
        write_before_pinmode_violations = 0;
        on_gpio_event = nullptr;
        on_gpio_ctx = nullptr;
    }
};

inline MockArduinoState& mockArduino() {
    static MockArduinoState state;
    return state;
}

static inline void mockAdvanceMillis(unsigned long delta_ms) {
    mockArduino().clock_ms += delta_ms;
    mockArduino().clock_us += static_cast<unsigned long long>(delta_ms) * 1000ULL;
}

static inline unsigned long millis() {
    return mockArduino().clock_ms;
}

static inline unsigned long micros() {
    return static_cast<unsigned long>(mockArduino().clock_us);
}

static inline void delay(unsigned long ms) {
    mockAdvanceMillis(ms);
}

static inline void delayMicroseconds(unsigned int us) {
    mockArduino().clock_us += us;
}

static inline void pinMode(uint8_t pin, uint8_t mode) {
    MockArduinoState& st = mockArduino();
    st.pin_mode_calls++;
    if (pin < 64) {
        st.pin_initialized[pin] = true;
        st.pin_mode[pin] = mode;
        if (mode == INPUT_PULLUP) {
            st.pin_level[pin] = HIGH;
        }
        if (st.on_gpio_event) {
            st.on_gpio_event(pin, st.pin_mode[pin], st.pin_level[pin], st.on_gpio_ctx);
        }
    }
}

static inline void digitalWrite(uint8_t pin, uint8_t val) {
    MockArduinoState& st = mockArduino();
    st.digital_write_calls++;
    if (pin < 64) {
        if (!st.pin_initialized[pin]) {
            st.write_before_pinmode_violations++;
        }
        st.pin_level[pin] = (val != LOW) ? HIGH : LOW;
        if (st.on_gpio_event) {
            st.on_gpio_event(pin, st.pin_mode[pin], st.pin_level[pin], st.on_gpio_ctx);
        }
    }
}

#endif // MOCK_ARDUINO_H
