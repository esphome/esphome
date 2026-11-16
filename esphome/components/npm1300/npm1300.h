#pragma once

// Product specification: https://docs.nordicsemi.com/bundle/ps_npm1300/page/keyfeatures_html5.html

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
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

/// Register codes for one regulator, NPM1300_UNSET where the YAML leaves the field out
struct NPM1300OutputConfig {
  uint8_t voltage;   // VOUT: (millivolts - 1000) / 100
  uint8_t ldo_mode;  // LDSWxLDOSEL, LDO1 and LDO2 only: 1 LDO, 0 load switch
  uint8_t enabled;   // turned on (1) or off (0) at boot
};

/// Register codes for the battery charger
struct NPM1300ChargerConfig {
  uint16_t current;                  // BCHGISET: milliamperes / 2
  uint16_t discharge_limit;          // BCHGISETDISCHARGE: 84 for 200 mA, 415 for 1000 mA
  uint8_t termination_voltage;       // BCHGVTERM
  uint8_t warm_termination_voltage;  // BCHGVTERMR
  uint8_t trickle_voltage;           // BCHGVTRICKLESEL: 0 for 2.9 V, 1 for 2.5 V
  uint8_t termination_current;       // BCHGITERMSEL: 0 for 10 %, 1 for 20 %
  bool recharge;
  bool enabled;
};

/// The settings applied in setup(), built by code generation into a PROGMEM table
struct NPM1300Config {
  NPM1300OutputConfig outputs[NPM1300_OUTPUT_COUNT];
  NPM1300ChargerConfig charger;  // only applied when charger_configured
  uint16_t thermistor_beta;
  uint8_t thermistor;  // ADCNTCRSEL code: 1 for 10 kOhm, 2 for 47 kOhm, 3 for 100 kOhm, or NPM1300_UNSET
  bool charger_configured;
};

#ifdef USE_NPM1300_DATA_PUBLISHER
/// The 10-bit ADC result whose high 8 bits are adc[msb] and low 2 bits are at `shift` in adc[lsbs]
inline uint16_t adc_code(const uint8_t *adc, uint8_t msb, uint8_t lsbs, uint8_t shift) {
  return (adc[msb] << 2) | ((adc[lsbs] >> shift) & 0x03);
}
/// Battery temperature in degrees Celsius from the NTC code and 1 / B constant of the thermistor
float ntc_temperature(uint16_t code, float inv_beta);
#endif
#ifdef USE_TEXT_SENSOR
/// Publish the charger status text, only when it differs from `last`
void publish_charger_status(text_sensor::TextSensor *sensor, uint8_t &last, uint8_t vbus_status, uint8_t charge_status,
                            uint8_t error_reason);
#endif

class NPM1300Component : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(vbus_current_limit)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(charger)
  SUB_SWITCH(buck1)
  SUB_SWITCH(buck2)
  SUB_SWITCH(ldo1)
  SUB_SWITCH(ldo2)
#endif

 public:
  /// `config` points into PROGMEM
  explicit NPM1300Component(const NPM1300Config *config) : config_(config) {}

  void setup() override;
  void update() override;
  void dump_config() override;
  // Right after the I2C bus: the regulators may power other peripherals
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

#ifdef USE_NPM1300_STATUS_PUBLISHER
  HUB_PUBLISHER(status, uint8_t, uint8_t, uint8_t)
#endif
#ifdef USE_NPM1300_DATA_PUBLISHER
  HUB_PUBLISHER(data, const uint8_t *, float)
#endif
#if defined(USE_NPM1300_STATUS_PUBLISHER) || defined(USE_NPM1300_DATA_PUBLISHER)
  HUB_PUBLISHER(dump, const char *)
#endif

  /// VBUSINILIM0 code: 100 mA per step (code 0, the reset value, is 500 mA)
  void set_vbus_current_limit(uint8_t code) { this->vbus_current_limit_ = code; }

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
  NPM1300Config load_config_() const;
  bool apply_vbus_current_limit_();
  bool configure_charger_(const NPM1300Config &config);
  bool configure_output_(uint8_t index, const NPM1300OutputConfig &config);
#ifdef USE_NPM1300_DATA_PUBLISHER
  bool read_charge_currents_();
  void read_measurements_();
#endif
#ifdef USE_SWITCH
  /// Returns false when a status read failed
  bool publish_switches_();
#endif

  const NPM1300Config *config_;
#ifdef USE_NPM1300_DATA_PUBLISHER
  // BCHGISET code and discharge limit, for the full scale of the battery current measurement
  uint16_t charge_code_{0};
  bool discharge_limit_high_{false};
#endif
  uint8_t vbus_current_limit_{NPM1300_UNSET};
  uint8_t last_error_reason_{0};
  bool vbus_present_{false};
};

}  // namespace esphome::npm1300
