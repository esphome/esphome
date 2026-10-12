#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::ds1603l {

class DS1603L final : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

 protected:
  static constexpr uint8_t HEADER_BYTE = 0xFF;
  static constexpr size_t FRAME_SIZE = 4;

  // Validates the checksum of the frame in rx_buffer_ and publishes it. Returns false if the frame is invalid.
  bool parse_data_();
  // Drops the first buffered byte and realigns the buffer on the next possible header byte.
  void resync_();

  uint8_t rx_buffer_[FRAME_SIZE];  // Buffer for the frame being assembled
  size_t rx_count_{0};             // Number of bytes currently in rx_buffer_
};

}  // namespace esphome::ds1603l
