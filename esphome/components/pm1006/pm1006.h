#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pm1006 {

class PM1006Component final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(pm_2_5)

 public:
  PM1006Component() = default;

  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  static constexpr uint8_t FRAME_SIZE = 20;

  void parse_data_(const uint8_t *frame);

  FrameReader<FRAME_SIZE, 0x16, 0x11, 0x0B> reader_;
  uint32_t last_transmission_{0};
};

}  // namespace esphome::pm1006
