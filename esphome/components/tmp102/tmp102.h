#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_TMP102_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/i2c/i2c.h"
#ifdef USE_TMP102_NUMBER
#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#endif
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TMP102_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cstdint>

namespace esphome::tmp102 {

#ifdef USE_HOST
namespace testing {
class TMP102Test;
}
#endif

#ifdef USE_TMP102_NUMBER
enum TMP102LimitType : uint8_t {
  TMP102_LIMIT_LOW = 0,
  TMP102_LIMIT_HIGH = 1,
};

class TMP102LimitNumber;
#endif

class TMP102Component final : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
#ifdef USE_TMP102_BINARY_SENSOR
  SUB_BINARY_SENSOR(alert)
#endif
#ifdef USE_TMP102_TEXT_SENSOR
  SUB_TEXT_SENSOR(threshold_status)
#endif
#if defined(USE_TMP102_CONFIGURE) || defined(USE_TMP102_TEXT_SENSOR)
  void setup() override;
#endif
#ifdef USE_TMP102_CONFIGURE
  void set_configuration(uint16_t config, uint16_t high_limit, uint16_t low_limit) {
    this->config_ = config;
    this->high_limit_ = high_limit;
    this->low_limit_ = low_limit;
    this->configure_ = true;
  }
#ifdef USE_TMP102_NUMBER
  void set_limit_control(TMP102LimitNumber *number);
#endif
#endif
  void dump_config() override;
  void update() override;

 protected:
#ifdef USE_TMP102_TEXT_SENSOR
  void publish_threshold_status_(uint8_t status);
  void publish_threshold_rejection_(bool high, float value, float other);
#else
  void publish_threshold_status_(uint8_t) {}
  void publish_threshold_rejection_(bool, float, float) {}
#endif
  void set_warning_and_invalidate_alert_() {
    this->status_set_warning();
#ifdef USE_TMP102_BINARY_SENSOR
    this->invalidate_alert_();
#endif
  }
#ifdef USE_TMP102_BINARY_SENSOR
  bool read_alert_state_();
  void invalidate_alert_();
#endif
#ifdef USE_TMP102_CONFIGURE
  void setup_configured_();
  bool write_register_(uint8_t reg, uint16_t value);
  bool write_configuration_();
  bool check_configuration_();
  void read_temperature_();
  void update_configured_();
#ifdef USE_TMP102_NUMBER
  friend class TMP102LimitNumber;
  bool set_limit_temperature_(TMP102LimitType limit, float temperature);
  float get_limit_temperature_(TMP102LimitType limit) const;
  bool prepare_number_limits_(float *restored, bool *restore_corrected);

  TMP102LimitNumber *limit_controls_[2]{};
#endif
  uint16_t config_{0};
  uint16_t high_limit_{0};
  uint16_t low_limit_{0};
  bool configure_{false};
#endif
};

#ifdef USE_TMP102_NUMBER
class TMP102LimitNumber final : public number::Number, public Parented<TMP102Component> {
 public:
  explicit TMP102LimitNumber(TMP102LimitType limit_type) : limit_type_(limit_type) {}
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  friend class TMP102Component;
#ifdef USE_HOST
  friend class testing::TMP102Test;
#endif
  float load_value_(float fallback, bool &corrected);
  bool save_value_(float value);
  void control(float value) override;

  ESPPreferenceObject pref_;
  TMP102LimitType limit_type_;
  bool restore_value_{true};
};
#endif

}  // namespace esphome::tmp102
