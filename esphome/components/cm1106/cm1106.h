#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
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
  void set_abc_enabled(bool abc_enabled) {
    this->abc_boot_logic_ = abc_enabled ? CM1106_ABC_ENABLED : CM1106_ABC_DISABLED;
  }
  void set_abc_cycle(uint8_t cycle) { this->abc_cycle_ = cycle; }
  void set_abc_baseline(uint16_t baseline) { this->abc_baseline_ = baseline; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};

  bool cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len);

  CM1106ABCLogic abc_boot_logic_{CM1106_ABC_NONE};
  uint8_t abc_cycle_{15};       // calibration cycle in days
  uint16_t abc_baseline_{400};  // baseline in ppm

 private:
  void abc_set_(bool enabled);
};

template<typename... Ts> class CM1106CalibrateZeroAction final : public Action<Ts...> {
 public:
  CM1106CalibrateZeroAction(CM1106Component *cm1106) : cm1106_(cm1106) {}

  void play(const Ts &...x) override { this->cm1106_->calibrate_zero(400); }

 protected:
  CM1106Component *cm1106_;
};

}  // namespace esphome::cm1106
