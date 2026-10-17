#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::uart {

// One per bus: reads the bytes once and presses each sensor whose pattern ends the stream
class UARTBinarySensor : public UARTDevice, public Component {
 public:
  void setup() override { this->buffer_.init(this->max_len_); }
  void loop() override;
  void dump_config() override;

  // The pattern lives in flash; called once per sensor by the generated code
  void add_event_matcher(binary_sensor::BinarySensor *sensor, const uint8_t *data, uint8_t data_len) {
    this->matchers_.push_back({sensor, data, data_len});
    this->max_len_ = std::max(this->max_len_, data_len);
  }

 protected:
  struct EventMatcher {
    binary_sensor::BinarySensor *sensor;
    const uint8_t *data;
    uint8_t data_len;
  };

  bool matches_(const EventMatcher &matcher) const;

  StaticVector<EventMatcher, UART_BINARY_SENSOR_COUNT> matchers_;
  FixedRingBuffer<uint8_t, 255> buffer_;
  uint8_t max_len_{0};
};

}  // namespace esphome::uart
