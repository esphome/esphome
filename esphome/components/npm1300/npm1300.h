#pragma once

// Product specification: https://docs.nordicsemi.com/bundle/ps_npm1300/page/keyfeatures_html5.html

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::npm1300 {

/// A thermistor setting the YAML leaves out
static constexpr uint8_t NPM1300_UNSET = 0xFF;

class NPM1300Component : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(battery_current)
  SUB_SENSOR(battery_temperature)
  SUB_SENSOR(die_temperature)
  SUB_SENSOR(system_voltage)
  SUB_SENSOR(vbus_voltage)
  SUB_SENSOR(vbus_current_limit)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(vbus_present)
  SUB_BINARY_SENSOR(vbus_current_limit_active)
  SUB_BINARY_SENSOR(charging)
  SUB_BINARY_SENSOR(charge_complete)
  SUB_BINARY_SENSOR(battery_detected)
  SUB_BINARY_SENSOR(charger_error)
#endif

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  /// ADCNTCRSEL code (1 for 10 kOhm, 2 for 47 kOhm, 3 for 100 kOhm) and the B constant of the battery thermistor
  void set_thermistor(uint8_t resistance, uint16_t beta) {
    this->thermistor_ = resistance;
    this->thermistor_beta_ = beta;
  }

 protected:
  bool write_reg_(uint16_t reg, uint8_t value) { return this->write_register16(reg, &value, 1) == i2c::ERROR_OK; }
  bool read_reg_(uint16_t reg, uint8_t *data, size_t len = 1) {
    return this->read_register16(reg, data, len) == i2c::ERROR_OK;
  }
  bool read_charge_currents_();
  void read_measurements_();
  void publish_status_(uint8_t vbus_status, uint8_t charge_status, uint8_t error_reason);

  // Full scale of the battery current measurement in milliamperes, from the charge and discharge limits
  float charge_full_scale_ma_{NAN};
  float discharge_full_scale_ma_{NAN};
  uint16_t thermistor_beta_{0};
  uint8_t thermistor_{NPM1300_UNSET};
  uint8_t last_error_reason_{0};
};

}  // namespace esphome::npm1300
