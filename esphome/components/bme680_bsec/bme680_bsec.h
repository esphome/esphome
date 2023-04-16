#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/preferences.h"
#include "esphome/core/defines.h"
#include <map>

#ifdef USE_BSEC
#include <bsec.h>
#endif

namespace esphome {
namespace bme680_bsec {
#ifdef USE_BSEC

enum IAQMode {
  IAQ_MODE_STATIC = 0,
  IAQ_MODE_MOBILE = 1,
};

enum SampleRate {
  SAMPLE_RATE_LP = 0,
  SAMPLE_RATE_ULP = 1,
  SAMPLE_RATE_DEFAULT = 2,
};

#define BME680_BSEC_SAMPLE_RATE_LOG(r) (r == SAMPLE_RATE_DEFAULT ? "Default" : (r == SAMPLE_RATE_ULP ? "ULP" : "LP"))

class BME680BSECComponent : public Component, public i2c::I2CDevice {
 public:
  void set_device_id(const std::string &devid) { this->device_id_.assign(devid); }
  void set_temperature_offset(float offset) { this->temperature_offset_ = offset; }
  void set_iaq_mode(IAQMode iaq_mode) { this->iaq_mode_ = iaq_mode; }
  void set_state_save_interval(uint32_t interval) { this->state_save_interval_ms_ = interval; }

  void set_sample_rate(SampleRate sample_rate) { this->sample_rate_ = sample_rate; }
  void set_temperature_sample_rate(SampleRate sample_rate) { this->temperature_sample_rate_ = sample_rate; }
  void set_pressure_sample_rate(SampleRate sample_rate) { this->pressure_sample_rate_ = sample_rate; }
  void set_humidity_sample_rate(SampleRate sample_rate) { this->humidity_sample_rate_ = sample_rate; }

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { this->pressure_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_gas_resistance_sensor(sensor::Sensor *sensor) { this->gas_resistance_sensor_ = sensor; }
  void set_iaq_sensor(sensor::Sensor *sensor) { this->iaq_sensor_ = sensor; }
  void set_iaq_accuracy_text_sensor(text_sensor::TextSensor *sensor) { this->iaq_accuracy_text_sensor_ = sensor; }
  void set_iaq_accuracy_sensor(sensor::Sensor *sensor) { this->iaq_accuracy_sensor_ = sensor; }
  void set_co2_equivalent_sensor(sensor::Sensor *sensor) { this->co2_equivalent_sensor_ = sensor; }
  void set_breath_voc_equivalent_sensor(sensor::Sensor *sensor) { this->breath_voc_equivalent_sensor_ = sensor; }

  static std::vector<BME680BSECComponent *> instances;
  static int8_t read_bytes_wrapper(uint8_t devid, uint8_t a_register, uint8_t *data, uint16_t len);
  static int8_t write_bytes_wrapper(uint8_t devid, uint8_t a_register, uint8_t *data, uint16_t len);
  static void delay_ms(uint32_t period);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

 protected:
  void set_config_();
  float calc_sensor_sample_rate_(SampleRate sample_rate);
  void update_subscription_();

  void run_();
  void read_();
  void publish_(const bsec_output_t *outputs, uint8_t num_outputs);
  int64_t get_time_ns_();

  void publish_sensor_(sensor::Sensor *sensor, float value, bool change_only = false);
  void publish_sensor_(text_sensor::TextSensor *sensor, const std::string &value);

  void snapshot_state_();  // Fetch the current BSEC library state and save it in the bsec_state_data_ member (volatile
                           // memory)
  void restore_state_();   // Push the state contained in the bsec_state_data_ member (volatile memory) to the BSEC
                           // library
  int reinit_bsec_lib_();  // Prepare the BSEC library to be used again after this object returns active
                           // (as the library may have been used by other objects)
  void load_state_();      // Initialize the ESP preferences object; retrieve the BSEC library state from the ESP
                           // preferences (storage); then save it in the bsec_state_data_ member (volatile memory) and
                           // push it to the BSEC library
  void save_state_(
      uint8_t accuracy);  // Save the bsec_state_data_ member (volatile memory) to the ESP preferences (storage)

  void queue_push_(std::function<void()> &&f) { this->queue_.push(std::move(f)); }

  static uint8_t work_buffer_[BSEC_MAX_WORKBUFFER_SIZE];
  struct bme680_dev bme680_;
  bsec_library_return_t bsec_status_{BSEC_OK};
  int8_t bme680_status_{BME680_OK};

  int64_t last_time_ms_{0};
  uint32_t millis_overflow_counter_{0};
  int64_t next_call_ns_{0};

  std::queue<std::function<void()>> queue_;

  bool bsec_state_data_valid_;
  uint8_t bsec_state_data_[BSEC_MAX_STATE_BLOB_SIZE];  // This is the current snapshot of the BSEC library state
  ESPPreferenceObject bsec_state_;
  uint32_t state_save_interval_ms_{21600000};  // 6 hours - 4 times a day
  uint32_t last_state_save_ms_ = 0;
  bsec_bme_settings_t bme680_settings_;

  std::string device_id_;
  float temperature_offset_{0};
  IAQMode iaq_mode_{IAQ_MODE_STATIC};

  SampleRate sample_rate_{SAMPLE_RATE_LP};  // Core/gas sample rate
  SampleRate temperature_sample_rate_{SAMPLE_RATE_DEFAULT};
  SampleRate pressure_sample_rate_{SAMPLE_RATE_DEFAULT};
  SampleRate humidity_sample_rate_{SAMPLE_RATE_DEFAULT};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
  sensor::Sensor *iaq_sensor_{nullptr};
  text_sensor::TextSensor *iaq_accuracy_text_sensor_{nullptr};
  sensor::Sensor *iaq_accuracy_sensor_{nullptr};
  sensor::Sensor *co2_equivalent_sensor_{nullptr};
  sensor::Sensor *breath_voc_equivalent_sensor_{nullptr};
};
#endif
}  // namespace bme680_bsec
}  // namespace esphome
