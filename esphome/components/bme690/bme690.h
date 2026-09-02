#pragma once

#include <array>
#include <memory>

extern "C" {
#include "bme69x.h"
#include "bsec_interface.h"
#include "bsec_datatypes.h"
#include "bsec_iaq.h"
}

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::bme690 {

inline bool bsec_has_input(uint32_t process_data, uint8_t input) { return (process_data & (1U << (input - 1))) != 0; }

enum SampleRate {
  SAMPLE_RATE_LP = 0,
  SAMPLE_RATE_ULP = 1,
};

class BME690Component : public Component, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { this->gas_resistance_sensor_ = sensor; }
  void set_iaq_sensor(sensor::Sensor *sensor) { this->iaq_sensor_ = sensor; }
  void set_iaq_accuracy_sensor(sensor::Sensor *sensor) { this->iaq_accuracy_sensor_ = sensor; }
  void set_static_iaq_sensor(sensor::Sensor *sensor) { this->static_iaq_sensor_ = sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *sensor) { this->co2_equivalent_sensor_ = sensor; }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *sensor) { this->breath_voc_equivalent_sensor_ = sensor; }
  void set_tvoc_equivalent_sensor(sensor::Sensor *sensor) { this->tvoc_equivalent_sensor_ = sensor; }
  void set_gas_percentage_sensor(sensor::Sensor *sensor) { this->gas_percentage_sensor_ = sensor; }
  void set_comp_temperature_sensor(sensor::Sensor *sensor) { this->comp_temperature_sensor_ = sensor; }
  void set_comp_humidity_sensor(sensor::Sensor *sensor) { this->comp_humidity_sensor_ = sensor; }
  void set_sample_rate(SampleRate sample_rate) { this->sample_rate_ = sample_rate; }
  void set_temperature_offset(float offset) { this->ext_temp_offset_ = offset; }
  void set_bsec_enabled(bool enabled) { this->bsec_enabled_ = enabled; }
  void set_bsec_configuration(const uint8_t *data, uint32_t len) {
    this->bsec_configuration_ = data;
    this->bsec_configuration_length_ = len;
  }
  void set_state_save_interval(uint32_t interval) { this->state_save_interval_ms_ = interval; }
  void set_state_preference_hash(uint32_t hash) { this->state_preference_hash_ = hash; }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ms_ = update_interval; }
#ifdef USE_TEXT_SENSOR
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *sensor) { this->iaq_accuracy_text_sensor_ = sensor; }
#endif

  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override;
  void dump_config() override;

 protected:
  static BME69X_INTF_RET_TYPE read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
  static BME69X_INTF_RET_TYPE write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
  static void delay_usec(uint32_t period, void *intf_ptr);
  bool check_result_(const char *label, int8_t rslt);
  bool check_bsec_status_(const char *label, bsec_library_return_t rslt);
  bool configure_bsec_();
  uint8_t build_iaq_subscription_(bsec_sensor_configuration_t *requested_virtual_sensors, float bsec_sample_rate) const;
  void read_();
  bool get_bsec_sensor_settings_(int64_t timestamp_ns, bsec_bme_settings_t *sensor_settings);
  void get_raw_fallback_settings_(bsec_bme_settings_t *sensor_settings);
  bool apply_sensor_settings_(bsec_bme_settings_t &sensor_settings);
  bool perform_measurement_(const bsec_bme_settings_t &sensor_settings, struct bme69x_data *data);
  void publish_raw_outputs_(const struct bme69x_data &data);
  void schedule_after_measurement_();
  void schedule_read_(uint32_t delay_ms);
  void schedule_next_bsec_read_();
  bool push_inputs_to_bsec_(const struct bme69x_data &data, const bsec_bme_settings_t &settings, int64_t timestamp_ns);
  void handle_bsec_outputs_(const bsec_output_t *outputs, uint8_t num_outputs);
  void log_bsec_version_();
  bool load_bsec_state_();
  void save_bsec_state_();
  int64_t get_time_ns_();
  bool is_bsec_3_3_or_newer_() const;
  float get_sample_rate_() const;
  uint8_t *bsec_instance_data_();
  const uint8_t *get_bsec_configuration_() const;
  uint32_t get_bsec_configuration_length_() const;

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
  sensor::Sensor *iaq_sensor_{nullptr};
  sensor::Sensor *iaq_accuracy_sensor_{nullptr};
  sensor::Sensor *static_iaq_sensor_{nullptr};
  sensor::Sensor *co2_equivalent_sensor_{nullptr};
  sensor::Sensor *breath_voc_equivalent_sensor_{nullptr};
  sensor::Sensor *tvoc_equivalent_sensor_{nullptr};
  sensor::Sensor *gas_percentage_sensor_{nullptr};
  sensor::Sensor *comp_temperature_sensor_{nullptr};
  sensor::Sensor *comp_humidity_sensor_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *iaq_accuracy_text_sensor_{nullptr};
#endif

  struct bme69x_dev dev_ {};
  struct bme69x_conf conf_ {};
  struct bme69x_heatr_conf heatr_conf_ {};
  std::unique_ptr<uint8_t[]> bsec_instance_;
  std::array<uint8_t, BSEC_MAX_WORKBUFFER_SIZE> bsec_work_buffer_{};
  const uint8_t *bsec_configuration_{nullptr};
  uint32_t bsec_configuration_length_{0};
  SampleRate sample_rate_{SAMPLE_RATE_LP};
  float ext_temp_offset_{0.0f};
  bool bsec_enabled_{false};
  bool bsec_ready_{false};
  bool bsec_fallback_warning_logged_{false};
  const char *bsec_setup_failed_step_{nullptr};
  bsec_library_return_t bsec_setup_failed_result_{BSEC_OK};
  bsec_version_t bsec_version_{};
  bool bsec_version_known_{false};
  int64_t next_call_ns_{0};
  ESPPreferenceObject pref_;
  uint32_t last_state_save_ms_{0};
  bool state_dirty_{false};
  uint32_t state_save_interval_ms_{6 * 60 * 60 * 1000UL};  // 6h
  uint32_t state_preference_hash_{0};
  uint32_t update_interval_ms_{3000};
};

}  // namespace esphome::bme690
