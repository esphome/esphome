#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome::vl6180x {

class VL6180XComponent final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(range)
  SUB_SENSOR(lux)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_scaler(uint8_t scaler) { this->scaler_ = scaler; }
  /// @param gain_code SYSALS_ANALOGUE_GAIN code of the gain
  /// @param integration_time_ms 1 to 512 ms
  /// @param lux_per_count lux of one ALS count at this gain and integration time
  void set_als(uint8_t gain_code, uint16_t integration_time_ms, float lux_per_count) {
    this->als_gain_code_ = gain_code;
    this->als_integration_time_ms_ = integration_time_ms;
    this->lux_per_count_ = lux_per_count;
  }

#ifdef VL6180X_ENABLE_PIN_COUNT
  /// The first setup() holds every registered sensor in reset, so each can be moved off 0x29 alone.
  static void register_enable_pin(GPIOPin *pin) { enable_pins.push_back(pin); }
#endif

 protected:
  bool init_();
  void start_range_();
  void read_range_();
  void start_als_();
  void read_als_();

#ifdef VL6180X_ENABLE_PIN_COUNT
  static SlotList<GPIOPin *, VL6180X_ENABLE_PIN_COUNT> enable_pins;  // NOLINT
#endif

  GPIOPin *enable_pin_{nullptr};
  float lux_per_count_{0.0f};
  uint16_t als_integration_time_ms_{100};
  uint8_t als_gain_code_{6};
  uint8_t scaler_{1};
  bool measuring_{false};
};

}  // namespace esphome::vl6180x
