#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sps30 {

/// This class implements support for the Sensirion SPS30 i2c/UART Particulate Matter
/// PM1.0, PM2.5, PM4, PM10 Air Quality sensors.
class SPS30Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(pmc_0_5)
  SUB_SENSOR(pmc_1_0)
  SUB_SENSOR(pmc_2_5)
  SUB_SENSOR(pmc_4_0)
  SUB_SENSOR(pmc_10_0)
  SUB_SENSOR(pm_size)

 public:
  void set_auto_cleaning_interval(uint32_t auto_cleaning_interval) { fan_interval_ = auto_cleaning_interval; }
  void set_idle_interval(uint32_t idle_interval) { idle_interval_ = idle_interval; }
  void setup() override;
  void update() override;
  void dump_config() override;

  bool start_fan_cleaning();
  bool stop_measurement();
  bool start_measurement();

 protected:
  bool setup_complete_{false};
  uint16_t raw_firmware_version_;
  char serial_number_[17] = {0};  /// Terminating NULL character
  uint8_t skipped_data_read_cycles_ = 0;
  uint32_t next_state_ms_ = 0;

  enum NextState : uint8_t { WAKE, READ, NONE } next_state_{NONE};

  bool start_continuous_measurement_();

  optional<uint32_t> fan_interval_;
  optional<uint32_t> idle_interval_;
};

}  // namespace esphome::sps30
