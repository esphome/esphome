#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::sm300d2 {

static constexpr uint8_t SM300D2_RESPONSE_LENGTH = 17;

class SM300D2Sensor final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(co2)
  SUB_SENSOR(formaldehyde)
  SUB_SENSOR(tvoc)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  FrameReader<SM300D2_RESPONSE_LENGTH, 0x3C, 0x02> reader_;
  uint8_t frame_[SM300D2_RESPONSE_LENGTH];
  bool has_frame_{false};
};

}  // namespace esphome::sm300d2
