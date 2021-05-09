

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/preferences.h"
#include <map>

#ifdef USING_BSEC
#include <bsec.h>
#endif

namespace esphome {
namespace bme680_bsec {
#ifdef USING_BSEC

enum IAQMode {
  IAQ_MODE_STATIC = 0,
  IAQ_MODE_MOBILE = 1,
};

enum SampleRate {
  SAMPLE_RATE_LP = 0,
  SAMPLE_RATE_ULP = 1,
};

class BME680BSECComponent : public Component, public i2c::I2CDevice {
 public:
  void set_temperature_offset(float offset) { this->temperature_offset_ = offset; }
  void set_iaq_mode(IAQMode iaq_mode) { this->iaq_mode_ = iaq_mode; }
  void set_sample_rate(SampleRate sample_rate) { this->sample_rate_ = sample_rate; }
  void set_state_save_interval(uint32_t interval) { this->state_save_interval_ms_ = interval; }

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *gas_resistance_sensor) {
    gas_resistance_sensor_ = gas_resistance_sensor;
  }
  void set_iaq_sensor(sensor::Sensor *iaq_sensor) { iaq_sensor_ = iaq_sensor; }
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *iaq_accuracy_text_sensor) {
    iaq_accuracy_text_sensor_ = iaq_accuracy_text_sensor;
  }
  void set_iaq_accuracy_sensor(sensor::Sensor *iaq_accuracy_sensor) { iaq_accuracy_sensor_ = iaq_accuracy_sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *co2_equivalent_sensor) {
    co2_equivalent_sensor_ = co2_equivalent_sensor;
  }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *breath_voc_equivalent_sensor) {
    breath_voc_equivalent_sensor_ = breath_voc_equivalent_sensor;
  }

  static BME680BSECComponent *instance;
  static int8_t read_bytes_wrapper(uint8_t address, uint8_t a_register, uint8_t *data, uint16_t len);
  static int8_t write_bytes_wrapper(uint8_t address, uint8_t a_register, uint8_t *data, uint16_t len);
  static void delay_ms(uint32_t period);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
  void set_config_(const uint8_t *config);
  void update_subscription_(float sample_rate);

  void run_();
  void read_(bsec_bme_settings_t bme680_settings);
  void publish_(const bsec_output_t *outputs, uint8_t num_outputs);
  int64_t get_time_ns_();

  void publish_sensor_state_(sensor::Sensor *sensor, float value, bool change_only = false);
  void publish_sensor_state_(text_sensor::TextSensor *sensor, std::string value);

  void load_state_();
  void save_state_(uint8_t accuracy);

  struct bme680_dev bme680_;
  bsec_library_return_t bsec_status_{BSEC_OK};
  int8_t bme680_status_{BME680_OK};

  int64_t last_time_ms_{0};
  uint32_t millis_overflow_counter_{0};
  int64_t next_call_ns_{0};

  ESPPreferenceObject bsec_state_;
  uint32_t state_save_interval_ms_{21600000};  // 6 hours - 4 times a day
  uint32_t last_state_save_ms_ = 0;

  float temperature_offset_{0};
  IAQMode iaq_mode_{IAQ_MODE_STATIC};
  SampleRate sample_rate_{SAMPLE_RATE_LP};

  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;
  sensor::Sensor *humidity_sensor_;
  sensor::Sensor *gas_resistance_sensor_;
  sensor::Sensor *iaq_sensor_;
  text_sensor::TextSensor *iaq_accuracy_text_sensor_;
  sensor::Sensor *iaq_accuracy_sensor_;
  sensor::Sensor *co2_equivalent_sensor_;
  sensor::Sensor *breath_voc_equivalent_sensor_;
};
#endif
}  // namespace bme680_bsec
}  // namespace esphome
