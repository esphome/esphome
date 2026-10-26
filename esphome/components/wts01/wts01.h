#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::wts01 {

constexpr uint8_t PACKET_SIZE = 9;

class WTS01Sensor final : public sensor::Sensor, public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;

 protected:
  void process_packet_(const uint8_t *packet);

  FrameReader<PACKET_SIZE, 0x55, 0x01, 0x01, 0x04> reader_;
};

}  // namespace esphome::wts01
