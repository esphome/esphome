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
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::npm1300 {

/// The regulators, in the order of their enable task registers
enum class NPM1300Output : uint8_t {
  NPM1300_OUTPUT_BUCK1 = 0,
  NPM1300_OUTPUT_BUCK2 = 1,
  NPM1300_OUTPUT_LDO1 = 2,
  NPM1300_OUTPUT_LDO2 = 3,
};
static constexpr uint8_t NPM1300_OUTPUT_COUNT = 4;

/// A configuration field the YAML leaves out, so the chip keeps its own value
static constexpr uint8_t NPM1300_UNSET = 0xFF;

/// Register codes for one regulator, packed by code generation
struct NPM1300OutputConfig {
  uint8_t voltage{NPM1300_UNSET};   // VOUT: (millivolts - 1000) / 100
  uint8_t ldo_mode{NPM1300_UNSET};  // LDSWxLDOSEL, LDO1 and LDO2 only: 1 LDO, 0 load switch
  uint8_t enabled{NPM1300_UNSET};   // turned on (1) or off (0) at boot
};

/// Register codes for the battery charger, packed by code generation
struct NPM1300ChargerConfig {
  uint16_t current{16};                 // BCHGISET: milliamperes / 2
  uint16_t discharge_limit{415};        // BCHGISETDISCHARGE: 84 for 200 mA, 415 for 1000 mA
  uint8_t termination_voltage{2};       // BCHGVTERM
  uint8_t warm_termination_voltage{2};  // BCHGVTERMR
  uint8_t trickle_voltage{0};           // BCHGVTRICKLESEL: 0 for 2.9 V, 1 for 2.5 V
  uint8_t termination_current{0};       // BCHGITERMSEL: 0 for 10 %, 1 for 20 %
  bool recharge{true};
  bool enabled{true};
};

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
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(charger_status)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(charger)
  SUB_SWITCH(buck1)
  SUB_SWITCH(buck2)
  SUB_SWITCH(ldo1)
  SUB_SWITCH(ldo2)
#endif

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  /// VBUSINILIM0 code: 1 for 100 mA, 5 to 15 for 500 mA to 1500 mA
  void set_vbus_current_limit(uint8_t code) { this->vbus_current_limit_ = code; }
  void set_charger_config(const NPM1300ChargerConfig &config) {
    this->charger_ = config;
    this->charger_configured_ = true;
  }
  void set_output_config(NPM1300Output output, const NPM1300OutputConfig &config) {
    this->outputs_[static_cast<uint8_t>(output)] = config;
  }
  /// ADCNTCRSEL code (1 for 10 kOhm, 2 for 47 kOhm, 3 for 100 kOhm) and the B constant of the battery thermistor
  void set_thermistor(uint8_t resistance, uint16_t beta) {
    this->thermistor_ = resistance;
    this->thermistor_beta_ = beta;
  }

  /// Turn the battery charger on or off; turning it on also clears a charger error
  bool set_charger_enabled(bool enabled);
  /// Turn a regulator on or off
  bool set_output_enabled(NPM1300Output output, bool enabled);
  /// Enter ship mode: every output is turned off until the SHPHLD button is pressed or VBUS is connected
  void enter_ship_mode();
  /// Enter hibernate mode: every output is turned off for `duration_ms`, or until the SHPHLD button is
  /// pressed or VBUS is connected
  void hibernate(uint32_t duration_ms);

 protected:
  bool write_reg_(uint16_t reg, uint8_t value) { return this->write_register16(reg, &value, 1) == i2c::ERROR_OK; }
  bool read_reg_(uint16_t reg, uint8_t *data, size_t len = 1) {
    return this->read_register16(reg, data, len) == i2c::ERROR_OK;
  }
  bool update_register16_(uint16_t reg, uint8_t mask, uint8_t value);
  bool apply_vbus_current_limit_();
  bool configure_charger_();
  bool configure_output_(uint8_t index, const NPM1300OutputConfig &config);
  bool read_charge_currents_();
  void read_measurements_();
  void publish_status_(uint8_t vbus_status, uint8_t charge_status, uint8_t error_reason);
#ifdef USE_SWITCH
  void publish_switches_();
#endif

  NPM1300OutputConfig outputs_[NPM1300_OUTPUT_COUNT]{};
  NPM1300ChargerConfig charger_{};
  // Full scale of the battery current measurement in milliamperes, from the charge and discharge limits
  float charge_full_scale_ma_{NAN};
  float discharge_full_scale_ma_{NAN};
  uint16_t thermistor_beta_{0};
  uint8_t vbus_current_limit_{NPM1300_UNSET};
  uint8_t thermistor_{NPM1300_UNSET};
  uint8_t last_error_reason_{0};
  bool charger_configured_{false};
};

}  // namespace esphome::npm1300
