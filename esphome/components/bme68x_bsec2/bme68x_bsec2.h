#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/preferences.h"

#ifdef USE_BSEC2

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cinttypes>
#include <queue>

#include <bsec2.h>

namespace esphome {
namespace bme68x_bsec2 {

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

class BME68xBSEC2Component : public Component {
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

  void set_bsec2_configuration(const uint8_t *data, const uint32_t len) {
    this->bsec2_configuration_ = data;
    this->bsec2_configuration_length_ = len;
  }

  void set_state_save_interval(uint32_t interval) { this->state_save_interval_ms_ = interval; }

#ifdef USE_SENSOR
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { this->gas_resistance_sensor_ = sensor; }
  void set_iaq_sensor(sensor::Sensor *sensor) { this->iaq_sensor_ = sensor; }
  void set_iaq_static_sensor(sensor::Sensor *sensor) { this->iaq_static_sensor_ = sensor; }
  void set_iaq_accuracy_sensor(sensor::Sensor *sensor) { this->iaq_accuracy_sensor_ = sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *sensor) { this->co2_equivalent_sensor_ = sensor; }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *sensor) { this->breath_voc_equivalent_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *sensor) { this->iaq_accuracy_text_sensor_ = sensor; }
#endif
  virtual uint32_t get_hash() = 0;

 protected:
  void set_config_(const uint8_t *config, u_int32_t len);
  float calc_sensor_sample_rate_(SampleRate sample_rate);
  void update_subscription_();

  void run_();
  void read_(int64_t trigger_time_ns);
  void publish_(const bsec_output_t *outputs, uint8_t num_outputs);
  int64_t get_time_ns_();

#ifdef USE_SENSOR
  void publish_sensor_(sensor::Sensor *sensor, float value, bool change_only = false);
#endif
#ifdef USE_TEXT_SENSOR
  void publish_sensor_(text_sensor::TextSensor *sensor, const std::string &value);
#endif

  void load_state_();
  void save_state_(uint8_t accuracy);

  void queue_push_(std::function<void()> &&f) { this->queue_.push(std::move(f)); }

  struct bme68x_dev bme68x_;
  bsec_bme_settings_t bsec_settings_;
  bsec_version_t version_;
  uint8_t bsec_instance_[BSEC_INSTANCE_SIZE];

  struct bme68x_heatr_conf bme68x_heatr_conf_;
  uint8_t op_mode_;  // operating mode of sensor
  bsec_library_return_t bsec_status_{BSEC_OK};
  int8_t bme68x_status_{BME68X_OK};

  int64_t last_time_ms_{0};
  uint32_t millis_overflow_counter_{0};

  std::queue<std::function<void()>> queue_;

  uint8_t const *bsec2_configuration_{nullptr};
  uint32_t bsec2_configuration_length_{0};
  bool bsec2_blob_configured_{false};

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

#ifdef USE_SENSOR
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
  sensor::Sensor *iaq_sensor_{nullptr};
  sensor::Sensor *iaq_static_sensor_{nullptr};
  sensor::Sensor *iaq_accuracy_sensor_{nullptr};
  sensor::Sensor *co2_equivalent_sensor_{nullptr};
  sensor::Sensor *breath_voc_equivalent_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *iaq_accuracy_text_sensor_{nullptr};
#endif
};

}  // namespace bme68x_bsec2
}  // namespace esphome
#endif
