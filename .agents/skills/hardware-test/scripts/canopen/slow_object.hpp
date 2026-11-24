#pragma once
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/canopen/od.h"

// OD handlers that answer late: the delay holds the SDO server (and its loop) past the client's response timeout
struct SlowObject : esphome::canopen::OdObject {
  bool slow_write{false};
  bool slow_read{false};
  uint32_t delay_ms{450};
  void wait_(const char *what) {
    esphome::delay(this->delay_ms);
    ESP_LOGI("nodeb", "slow %s handled after %u ms", what, (unsigned) this->delay_ms);
  }
  esphome::canopen::OdResult od_read(esphome::canopen::OdStream s, void *buf, size_t count, size_t *done) {
    if (this->slow_read && s.offset() == 0)
      this->wait_("read");
    return s.read_original(buf, count, done);
  }
  esphome::canopen::OdResult od_write(esphome::canopen::OdStream s, const void *buf, size_t count, size_t *done) {
    if (this->slow_write)
      this->wait_("write");
    return s.write_original(buf, count, done);
  }
};
