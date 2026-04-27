#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"

namespace esphome::uart {

class UARTBinarySensor : public UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void add_event_matcher(binary_sensor::BinarySensor *sensor, const uint8_t *data, size_t data_len);
  void setup_buffer(size_t max_matcher_len) { this->buffer_.init(max_matcher_len); }

 protected:
  struct EventMatcher {
    binary_sensor::BinarySensor *sensor;
    const uint8_t *data;
    size_t data_len;
    bool triggered{false};
  };

  void read_data_();
  FixedVector<EventMatcher> matchers_;
  FixedRingBuffer<uint8_t> buffer_;
};

}  // namespace esphome::uart
