#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_TMP102_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/i2c/i2c.h"
#ifdef USE_TMP102_NUMBER
#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"
#endif
#include "esphome/components/sensor/sensor.h"

#ifdef USE_TMP102_NUMBER
#include <cmath>
#endif
#include <cstdint>

namespace esphome::tmp102 {

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
#ifdef USE_TMP102_CONFIGURE
  void setup() override;
  void set_configuration(uint16_t config, uint16_t high_limit, uint16_t low_limit) {
    this->config_ = config;
    this->high_limit_ = high_limit;
    this->low_limit_ = low_limit;
    this->configure_ = true;
  }
#ifdef USE_TMP102_NUMBER
  void set_high_limit_control(TMP102LimitNumber *number) { this->high_limit_control_ = number; }
  void set_low_limit_control(TMP102LimitNumber *number) { this->low_limit_control_ = number; }
#endif
#endif
  void dump_config() override;
  void update() override;

 protected:
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
  bool write_register_(uint8_t reg, uint16_t value);
  bool write_configuration_();
  bool check_configuration_();
  void read_temperature_();
  void update_configured_();
#ifdef USE_TMP102_NUMBER
  friend class TMP102LimitNumber;
  bool set_limit_temperature_(TMP102LimitType limit, float temperature);
  float get_limit_temperature_(TMP102LimitType limit) const;
  bool validate_limit_temperature_(TMP102LimitType limit, float temperature) const;
  void prepare_number_limits_();

  TMP102LimitNumber *high_limit_control_{nullptr};
  TMP102LimitNumber *low_limit_control_{nullptr};
  bool threshold_write_failed_{false};
#endif

  uint16_t config_{0};
  uint16_t high_limit_{0};
  uint16_t low_limit_{0};
  bool configure_{false};
#endif
};

#ifdef USE_TMP102_NUMBER
class TMP102LimitNumber final : public number::Number, public Component {
 public:
  TMP102LimitNumber(TMP102Component *parent, TMP102LimitType limit_type) : parent_(parent), limit_type_(limit_type) {}
  void setup() override;
  void dump_config() override;
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  friend class TMP102Component;
  float restore_value();
  bool save_value(float value);
  void control(float value) override;

  TMP102Component *parent_;
  TMP102LimitType limit_type_;
  float initial_value_{NAN};
  bool restore_value_{true};
  ESPPreferenceObject pref_;
};
#endif

}  // namespace esphome::tmp102
