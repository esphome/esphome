#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::cm1106 {

// ABC logic requested at boot; CM1106_ABC_NONE means nothing is sent and the
// sensor keeps its stored setting.
enum CM1106ABCLogic : uint8_t {
  CM1106_ABC_NONE = 0,
  CM1106_ABC_ENABLED,
  CM1106_ABC_DISABLED,
};

class CM1106Component final : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate_zero(uint16_t ppm);
  void abc_enable() { this->abc_set_(true); }
  void abc_disable() { this->abc_set_(false); }

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_abc(bool enabled, uint8_t cycle_days, uint16_t baseline_ppm) {
    this->abc_boot_logic_ = enabled ? CM1106_ABC_ENABLED : CM1106_ABC_DISABLED;
    this->abc_cycle_ = cycle_days;
    this->abc_baseline_ = baseline_ppm;
  }

 protected:
  bool cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len);
  /// Sends a command and checks the sensor's four byte acknowledgement, updating the warning status.
  bool send_command_expect_ack_(const uint8_t *command, size_t command_len, const uint8_t *ack);
  void abc_set_(bool enabled);

  sensor::Sensor *co2_sensor_{nullptr};
  CM1106ABCLogic abc_boot_logic_{CM1106_ABC_NONE};
  uint8_t abc_cycle_{15};
  uint16_t abc_baseline_{400};
};

}  // namespace esphome::cm1106
