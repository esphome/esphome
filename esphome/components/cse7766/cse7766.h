#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::cse7766 {

class CSE7766Component final : public Component, public uart::UARTDevice {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)
  SUB_SENSOR(energy)
  SUB_SENSOR(apparent_power)
  SUB_SENSOR(reactive_power)
  SUB_SENSOR(power_factor)

 public:
  void loop() override;
  void dump_config() override;

 protected:
  static constexpr size_t RAW_DATA_SIZE = 24;

  bool check_byte_(uint8_t index, uint8_t byte) const;
  /// Returns false when the frame reports an error instead of measurements.
  bool parse_data_();

  uint8_t raw_data_[RAW_DATA_SIZE];
  uint8_t raw_data_index_{0};
  uint16_t cf_pulses_last_{0};
  uint32_t last_transmission_{0};
  uint32_t cf_pulses_total_{0};
  // Power cycle of the last frame that carried a new power measurement, 0 if none
  uint32_t last_power_cycle_{0};
};

}  // namespace esphome::cse7766
