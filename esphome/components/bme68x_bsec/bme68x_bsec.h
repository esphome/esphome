#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/preferences.h"
#include "esphome/core/defines.h"

#include <cinttypes>
#include <map>

#ifdef USE_BSEC2
#include <bsec2.h>
#endif

namespace esphome {
namespace bme68x_bsec {
#ifdef USE_BSEC2

enum AlgorithmOutput {
  ALGORITHM_OUTPUT_IAQ,
  ALGORITHM_OUTPUT_CLASSIFICATION,
  ALGORITHM_OUTPUT_REGRESSION,
};

enum OperatingAge {
  OPERATING_AGE_4D,
  OPERATING_AGE_28D,
};

enum SampleRate {
  SAMPLE_RATE_LP = 0,
  SAMPLE_RATE_ULP = 1,
  SAMPLE_RATE_DEFAULT = 2,
};

enum Voltage {
  VOLTAGE_1_8V,
  VOLTAGE_3_3V,
};

class BME68XBSECComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  void set_algorithm_output(AlgorithmOutput algorithm_output) { this->algorithm_output_ = algorithm_output; }
  void set_operating_age(OperatingAge operating_age) { this->operating_age_ = operating_age; }
  void set_temperature_offset(float offset) { this->temperature_offset_ = offset; }
  void set_voltage(Voltage voltage) { this->voltage_ = voltage; }

  void set_sample_rate(SampleRate sample_rate) { this->sample_rate_ = sample_rate; }
  void set_temperature_sample_rate(SampleRate sample_rate) { this->temperature_sample_rate_ = sample_rate; }
  void set_pressure_sample_rate(SampleRate sample_rate) { this->pressure_sample_rate_ = sample_rate; }
  void set_humidity_sample_rate(SampleRate sample_rate) { this->humidity_sample_rate_ = sample_rate; }

  void set_bsec_configuration(const uint8_t *data, const uint32_t len) {
    this->bsec_configuration_ = data;
    this->bsec_configuration_length_ = len;
  }

  void set_state_save_interval(uint32_t interval) { this->state_save_interval_ms_ = interval; }

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { this->gas_resistance_sensor_ = sensor; }
  void set_iaq_sensor(sensor::Sensor *sensor) { this->iaq_sensor_ = sensor; }
  void set_iaq_static_sensor(sensor::Sensor *sensor) { this->iaq_static_sensor_ = sensor; }
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *sensor) { this->iaq_accuracy_text_sensor_ = sensor; }
  void set_iaq_accuracy_sensor(sensor::Sensor *sensor) { this->iaq_accuracy_sensor_ = sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *sensor) { this->co2_equivalent_sensor_ = sensor; }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *sensor) { this->breath_voc_equivalent_sensor_ = sensor; }

  static BME68XBSECComponent *instance;
  static int8_t read_bytes_wrapper(uint8_t a_register, uint8_t *data, uint32_t len, void *intfPtr);
  static int8_t write_bytes_wrapper(uint8_t a_register, const uint8_t *data, uint32_t len, void *intfPtr);
  static void delay_us(uint32_t period, void *intfPtr);

 protected:
  void set_config_(const uint8_t *config, u_int32_t len);
  float calc_sensor_sample_rate_(SampleRate sample_rate);
  void update_subscription_();

  void run_();
  void read_(int64_t trigger_time_ns);
  void publish_(const bsec_output_t *outputs, uint8_t num_outputs);
  int64_t get_time_ns_();

  void publish_sensor_(sensor::Sensor *sensor, float value, bool change_only = false);
  void publish_sensor_(text_sensor::TextSensor *sensor, const std::string &value);

  void load_state_();
  void save_state_(uint8_t accuracy);

  void queue_push_(std::function<void()> &&f) { this->queue_.push(std::move(f)); }

  struct bme68x_dev bme68x_;
  bsec_bme_settings_t bsec_settings;
  uint8_t bsec_instance_[BSEC_INSTANCE_SIZE];

  struct bme68x_heatr_conf bme68x_heatr_conf;
  /* operating mode of sensor */
  uint8_t op_mode;
  bool sleep_mode;
  bsec_library_return_t bsec_status_{BSEC_OK};
  int8_t bme68x_status_{BME68X_OK};

  int64_t last_time_ms_{0};
  uint32_t millis_overflow_counter_{0};
  int64_t next_call_ns_{0};

  std::queue<std::function<void()>> queue_;

  uint8_t const *bsec_configuration_{nullptr};
  uint32_t bsec_configuration_length_{0};

  ESPPreferenceObject bsec_state_;
  uint32_t state_save_interval_ms_{21600000};  // 6 hours - 4 times a day
  uint32_t last_state_save_ms_ = 0;

  float temperature_offset_{0};

  AlgorithmOutput algorithm_output_{ALGORITHM_OUTPUT_IAQ};
  OperatingAge operating_age_{OPERATING_AGE_28D};
  Voltage voltage_{VOLTAGE_3_3V};

  SampleRate sample_rate_{SAMPLE_RATE_LP};  // Core/gas sample rate
  SampleRate temperature_sample_rate_{SAMPLE_RATE_DEFAULT};
  SampleRate pressure_sample_rate_{SAMPLE_RATE_DEFAULT};
  SampleRate humidity_sample_rate_{SAMPLE_RATE_DEFAULT};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
  sensor::Sensor *iaq_sensor_{nullptr};
  sensor::Sensor *iaq_static_sensor_{nullptr};
  text_sensor::TextSensor *iaq_accuracy_text_sensor_{nullptr};
  sensor::Sensor *iaq_accuracy_sensor_{nullptr};
  sensor::Sensor *co2_equivalent_sensor_{nullptr};
  sensor::Sensor *breath_voc_equivalent_sensor_{nullptr};
};
#endif
}  // namespace bme68x_bsec
}  // namespace esphome
