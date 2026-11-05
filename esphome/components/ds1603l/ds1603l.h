#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"

namespace esphome::ds1603l {

class DS1603L final : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

 protected:
  static constexpr uint8_t HEADER_BYTE = 0xFF;
  static constexpr size_t FRAME_SIZE = 4;

  // Validates the checksum of the frame and publishes it. Returns false if the frame is invalid.
  bool parse_data_(const uint8_t *frame);

  FrameReader<FRAME_SIZE, HEADER_BYTE> reader_;
};

}  // namespace esphome::ds1603l
