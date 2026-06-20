#pragma once

#include <vector>
#include <array>

// Use floating point calculations provided by the Bosch driver.
#ifndef BME69X_USE_FPU
#define BME69X_USE_FPU
#endif

extern "C" {
#include "bme69x.h"
#include "bsec_interface.h"
#include "bsec_datatypes.h"
#include "bsec_iaq.h"
}

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace bme690 {

static const char *const TAG = "bme690";
static const char *const IAQ_ACCURACY_STATES[4] = {"Stabilizing", "Uncertain", "Calibrating", "Calibrated"};

inline bool bsec_has_input(uint32_t process_data, uint8_t input) {
  return (process_data & (1U << (input - 1))) != 0;
}

class BME690Component : public PollingComponent, public i2c::I2CDevice {
 public:
  explicit BME690Component(uint32_t update_interval = 5000) : PollingComponent(update_interval) {}

  void set_temperature_sensor(sensor::Sensor *sensor) { temperature_sensor = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { humidity_sensor = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { gas_resistance_sensor = sensor; }
  void set_iaq_sensor(sensor::Sensor *sensor) { iaq_sensor = sensor; }
  void set_iaq_accuracy_sensor(sensor::Sensor *sensor) { iaq_accuracy_sensor = sensor; }
  void set_static_iaq_sensor(sensor::Sensor *sensor) { static_iaq_sensor = sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *sensor) { co2_equivalent_sensor = sensor; }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *sensor) { breath_voc_equivalent_sensor = sensor; }
  void set_gas_percentage_sensor(sensor::Sensor *sensor) { gas_percentage_sensor = sensor; }
  void set_comp_temperature_sensor(sensor::Sensor *sensor) { comp_temperature_sensor = sensor; }
  void set_comp_humidity_sensor(sensor::Sensor *sensor) { comp_humidity_sensor = sensor; }
  void set_state_save_interval(uint32_t interval) { state_save_interval_ms_ = interval; }
  void set_state_preference_hash(uint32_t hash) { this->state_preference_hash_ = hash; }
#ifdef USE_TEXT_SENSOR
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *sensor) { iaq_accuracy_text_sensor_ = sensor; }
#endif

  sensor::Sensor *temperature_sensor{nullptr};
  sensor::Sensor *humidity_sensor{nullptr};
  sensor::Sensor *pressure_sensor{nullptr};
  sensor::Sensor *gas_resistance_sensor{nullptr};
  sensor::Sensor *iaq_sensor{nullptr};
  sensor::Sensor *iaq_accuracy_sensor{nullptr};
  sensor::Sensor *static_iaq_sensor{nullptr};
  sensor::Sensor *co2_equivalent_sensor{nullptr};
  sensor::Sensor *breath_voc_equivalent_sensor{nullptr};
  sensor::Sensor *gas_percentage_sensor{nullptr};
  sensor::Sensor *comp_temperature_sensor{nullptr};
  sensor::Sensor *comp_humidity_sensor{nullptr};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *iaq_accuracy_text_sensor_{nullptr};
#endif

  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  static BME69X_INTF_RET_TYPE read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
  static BME69X_INTF_RET_TYPE write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
  static void delay_usec(uint32_t period, void *intf_ptr);
  bool check_result_(const char *label, int8_t rslt);
  bool check_bsec_status_(const char *label, bsec_library_return_t rslt);
  bool configure_bsec_();
  bool push_inputs_to_bsec_(const struct bme69x_data &data, const bsec_bme_settings_t &settings, int64_t timestamp_ns);
  void handle_bsec_outputs_(const bsec_output_t *outputs, uint8_t num_outputs);
  void log_bsec_version_();
  bool load_bsec_state_();
  void save_bsec_state_();

  struct bme69x_dev dev_{};
  struct bme69x_conf conf_{};
  struct bme69x_heatr_conf heatr_conf_{};
  std::vector<uint8_t> bsec_instance_;
  std::vector<uint8_t> bsec_work_buffer_;
  float sample_rate_{BSEC_SAMPLE_RATE_ULP};
  float ext_temp_offset_{0.0f};
  bool bsec_ready_{false};
  int64_t next_call_ns_{0};
  ESPPreferenceObject pref_;
  uint32_t last_state_save_ms_{0};
  uint8_t last_iaq_accuracy_{0};
  bool state_dirty_{false};
  uint32_t state_save_interval_ms_{6 * 60 * 60 * 1000UL};  // 6h
  uint32_t state_preference_hash_{0};
};

}  // namespace bme690
}  // namespace esphome
