#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome::hx711 {

// The value is the number of extra clock pulses after a read, which selects the next conversion.
enum HX711Gain : uint8_t {
  HX711_GAIN_128 = 1,
  HX711_GAIN_32 = 2,
  HX711_GAIN_64 = 3,
};

class HX711Sensor final : public sensor::Sensor, public PollingComponent {
 public:
  void set_dout_pin(GPIOPin *dout_pin) { this->dout_pin_ = dout_pin; }
  void set_sck_pin(GPIOPin *sck_pin) { this->sck_pin_ = sck_pin; }
  void set_gain(HX711Gain gain) { this->gain_ = gain; }
  void set_settling_time(uint16_t settling_time_ms) { this->settling_time_ms_ = settling_time_ms; }
  void set_measurement_ready_timeout(uint16_t timeout_ms) { this->measurement_ready_timeout_ms_ = timeout_ms; }
  void set_power_down_after_reading(bool power_down) { this->power_down_after_reading_ = power_down; }
#ifdef USE_HX711_CHANNEL_B_SENSOR
  void set_channel_b_sensor(sensor::Sensor *channel_b_sensor) { this->channel_b_sensor_ = channel_b_sensor; }
#endif

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void on_powerdown() override;

  /// Wake the chip after power_down(); updates resume once it has settled.
  void power_up();
  /// Put the chip to sleep; updates are skipped until power_up().
  void power_down();

 protected:
  enum class Phase : uint8_t {
    SLEEPING,      // powered down by power_down()
    POWERED_DOWN,  // powered down between updates or after a failure
    WAKING,        // waiting for the first conversion after power-up
    SETTLING,      // waiting settling_time_ms_ after power-up or a gain change
    IDLE,
    READING,  // waiting for DOUT to signal a conversion
  };

  bool read_raw_(int32_t &value, HX711Gain next_gain);
  void power_on_();
  void power_off_(Phase phase);
  void fail_update_(const LogString *reason);
  bool timed_out_(uint32_t now) const { return now - this->phase_start_ > this->measurement_ready_timeout_ms_; }

  GPIOPin *dout_pin_;
  GPIOPin *sck_pin_;
#ifdef USE_HX711_CHANNEL_B_SENSOR
  sensor::Sensor *channel_b_sensor_{nullptr};
#endif
  uint32_t phase_start_{0};
  uint16_t settling_time_ms_{400};
  uint16_t measurement_ready_timeout_ms_{2000};
  HX711Gain gain_{HX711_GAIN_128};
  HX711Gain last_gain_{HX711_GAIN_128};  // gain the current conversion was started with
  Phase phase_{Phase::POWERED_DOWN};
  bool power_down_after_reading_{false};
  bool update_pending_{false};
#ifdef USE_HX711_CHANNEL_B_SENSOR
  bool channel_b_pending_{false};  // the next read returns the channel B value
#endif
};

}  // namespace esphome::hx711
