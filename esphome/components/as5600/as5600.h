#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace as5600 {

static const uint16_t POSITION_COUNT = 4096;
static const float RAW_TO_DEGREES = 360.0 / POSITION_COUNT;
static const float DEGREES_TO_RAW = POSITION_COUNT / 360.0;

enum EndPositionMode : uint8_t {
  // In this mode, the end position is calculated by taking the start position
  // and adding the range/positions. For example, you could say start at 90deg,
  // and have a range of 180deg and effectively the sensor will report values
  // from the physical 90deg thru 270deg.
  END_MODE_RANGE,
  // In this mode, the end position is explicitly set, and changing the start
  // position will NOT change the end position.
  END_MODE_POSITION,
};

enum OutRangeMode : uint8_t {
  // In this mode, the AS5600 chip itself actually reports these values, but
  // effectively it splits the out-of-range values in half, and when positioned
  // over the half closest to the min/start position, it will report 0 and when
  // positioned over the half closes to the max/end position, it will report the
  // max/end value.
  OUT_RANGE_MODE_MIN_MAX,
  // In this mode, when the magnet is positioned outside the configured
  // range, the sensor will report NAN, which translates to "Unknown"
  // in Home Assistant.
  OUT_RANGE_MODE_NAN,
};

enum AS5600MagnetStatus : uint8_t {
  MAGNET_GONE = 2,    // 0b010 / magnet not detected
  MAGNET_OK = 4,      // 0b100 / magnet just right
  MAGNET_STRONG = 5,  // 0b101 / magnet too strong
  MAGNET_WEAK = 6,    // 0b110 / magnet too weak
};

class AS5600Component : public Component, public i2c::I2CDevice {
 public:
  /// Set up the internal sensor array.
  void setup() override;
  void dump_config() override;
  /// HARDWARE_LATE setup priority
  float get_setup_priority() const override { return setup_priority::DATA; }

  // configuration setters
  void set_dir_pin(InternalGPIOPin *pin) { this->dir_pin_ = pin; }
  void set_direction(uint8_t direction) { this->direction_ = direction; }
  void set_fast_filter(uint8_t fast_filter) { this->fast_filter_ = fast_filter; }
  void set_hysteresis(uint8_t hysteresis) { this->hysteresis_ = hysteresis; }
  void set_power_mode(uint8_t power_mode) { this->power_mode_ = power_mode; }
  void set_slow_filter(uint8_t slow_filter) { this->slow_filter_ = slow_filter; }
  void set_watchdog(bool watchdog) { this->watchdog_ = watchdog; }
  bool get_watchdog() { return this->watchdog_; }
  void set_start_position(uint16_t start_position) { this->start_position_ = start_position % POSITION_COUNT; }
  void set_end_position(uint16_t end_position) {
    this->end_position_ = end_position % POSITION_COUNT;
    this->end_mode_ = END_MODE_POSITION;
  }
  void set_range(uint16_t range) {
    this->end_position_ = range % POSITION_COUNT;
    this->end_mode_ = END_MODE_RANGE;
  }

  // Gets the scale value for the configured range.
  // For example, if configured to start at 0deg and end at 180deg, the
  // range is 50% of the native/raw range, so the range scale would be 0.5.
  // If configured to use the full 360deg, the range scale would be 1.0.
  float get_range_scale() { return this->range_scale_; }

  // Indicates whether the given *raw* position is within the configured range
  bool in_range(uint16_t raw_position);

  AS5600MagnetStatus read_magnet_status();
  optional<uint16_t> read_position();
  optional<uint16_t> read_raw_position();

 protected:
  InternalGPIOPin *dir_pin_{nullptr};
  uint8_t direction_;
  uint8_t fast_filter_;
  uint8_t hysteresis_;
  uint8_t power_mode_;
  uint8_t slow_filter_;
  uint8_t pwm_frequency_{0};
  uint8_t output_mode_{0};
  bool watchdog_;
  uint16_t start_position_;
  uint16_t end_position_{0};
  uint16_t raw_max_;
  EndPositionMode end_mode_{END_MODE_RANGE};
  float range_scale_{1.0};
};

}  // namespace as5600
}  // namespace esphome
