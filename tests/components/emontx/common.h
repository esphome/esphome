#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/emontx/emontx.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::emontx::testing {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;

  void push_rx(const char *data) { this->rx.insert(this->rx.end(), data, data + strlen(data)); }
  void push_rx(size_t count, uint8_t byte) { this->rx.insert(this->rx.end(), count, byte); }

  // UARTComponent
  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }

    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }

  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)
};

inline void cancel_timers(EmonTx *emontx) {
  for (uint32_t id = 0; id < 3; id++)
    App.scheduler.cancel_timeout(emontx, id);
  App.scheduler.call(millis());
}

// Lets timeouts due within duration_ms run; the scheduler reads the real clock
inline void run_timers(uint32_t duration_ms) {
  App.scheduler.call(millis());
  delay(duration_ms);  // NOLINT - test only
  App.scheduler.call(millis());
}

}  // namespace esphome::emontx::testing
