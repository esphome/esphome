#pragma once
// A fake EZO circuit behind a fake I2C bus: the real ezo component runs unchanged and its bus pointer is swapped for
// this one, which logs every write and read with timestamps and answers like an EZO pH circuit.
#include <cstdio>
#include <cstring>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace fakeezo {
static const char *const TAG = "fakeezo";

class FakeEzoBus : public esphome::i2c::I2CBus {
 public:
  char last_[48]{};
  uint32_t write_ms_{0};
  uint32_t reads_{0};
  uint32_t pend_left_{0};
  uint32_t writes_{0};

  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *wb, size_t wn, uint8_t *rb, size_t rn) override {
    uint32_t now = esphome::millis();
    if (wn > 0) {
      size_t n = wn < sizeof(this->last_) - 1 ? wn : sizeof(this->last_) - 1;
      memcpy(this->last_, wb, n);
      this->last_[n] = 0;
      this->write_ms_ = now;
      this->writes_++;
      if (strcmp(this->last_, "pend") == 0)
        this->pend_left_ = 2;
      ESP_LOGI(TAG, "W 0x%02X \"%s\"", address, this->last_);
    }
    if (rn == 0)
      return esphome::i2c::ERROR_OK;
    uint32_t waited = now - this->write_ms_;
    if (strcmp(this->last_, "nak") == 0) {
      ESP_LOGI(TAG, "R 0x%02X after %u ms for \"%s\": NAK", address, (unsigned) waited, this->last_);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    memset(rb, 0, rn);
    rb[0] = 1;
    char *p = reinterpret_cast<char *>(rb + 1);
    size_t cap = rn - 1;
    const char *c = this->last_;
    if (strcmp(c, "R") == 0) {
      this->reads_++;
      if (this->reads_ % 3 == 0)
        snprintf(p, cap, "7.%03u,1.25", (unsigned) (this->reads_ % 1000));
      else
        snprintf(p, cap, "7.%03u", (unsigned) (this->reads_ % 1000));
    } else if (strcmp(c, "i") == 0) {
      snprintf(p, cap, "?I,pH,2.16");
    } else if (strcmp(c, "Slope,?") == 0) {
      snprintf(p, cap, "?Slope,99.7,100.3");
    } else if (strcmp(c, "Cal,?") == 0) {
      snprintf(p, cap, "?Cal,2");
    } else if (strcmp(c, "T,?") == 0) {
      snprintf(p, cap, "?T,25.00");
    } else if (strcmp(c, "L,?") == 0) {
      snprintf(p, cap, "?L,1");
    } else if (strcmp(c, "Name,?") == 0) {
      snprintf(p, cap, "?Name,ezo1");
    } else if (strcmp(c, "pend") == 0) {
      if (this->pend_left_ > 0) {
        this->pend_left_--;
        rb[0] = 254;
      } else {
        snprintf(p, cap, "done");
      }
    } else if (strcmp(c, "bad") == 0) {
      rb[0] = 2;
    } else if (strcmp(c, "nodata") == 0) {
      rb[0] = 255;
    } else if (strcmp(c, "full31") == 0) {
      // 31 payload bytes with no terminator anywhere in the 32 byte reply
      for (size_t i = 0; i < cap; i++)
        p[i] = 'A' + (i % 26);
    } else {
      // L,0 / L,1 / Cal,... / T,<v>: success with no data
    }
    ESP_LOGI(TAG, "R 0x%02X after %u ms for \"%s\": code %u", address, (unsigned) waited, this->last_, rb[0]);
    return esphome::i2c::ERROR_OK;
  }
};

static FakeEzoBus bus;
}  // namespace fakeezo
