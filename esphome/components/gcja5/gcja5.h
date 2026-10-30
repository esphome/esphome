#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::gcja5 {

class GCJA5Component final : public Component, public uart::UARTDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(pmc_0_3)
  SUB_SENSOR(pmc_0_5)
  SUB_SENSOR(pmc_1_0)
  SUB_SENSOR(pmc_2_5)
  SUB_SENSOR(pmc_5_0)
  SUB_SENSOR(pmc_10_0)

 public:
  void dump_config() override;
  void loop() override;

 protected:
  bool parse_data_(const uint8_t *frame);

  uint32_t last_transmission_{0};
  FrameReader<32, 0x02> reader_;

  bool first_status_log_{false};
};

}  // namespace esphome::gcja5
