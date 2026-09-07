#pragma once

#include <Arduino.h>

/** Count UTF-8 bytes that would be written (for MQTT sizing). */
class JsonCountingPrint final : public Print {
public:
    size_t written() const { return n_; }
    size_t write(uint8_t b) override {
        n_++;
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        n_ += size;
        return size;
    }
    void reset() { n_ = 0; }

private:
    size_t n_{0};
};
