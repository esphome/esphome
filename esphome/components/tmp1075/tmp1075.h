#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace tmp1075 {

struct TMP1075Config {
  union {
    struct {
      uint8_t oneshot : 1;  // One-shot conversion mode. Writing 1, starts a single temperature
                            // conversion. Read returns 0.

      uint8_t rate : 2;  // Conversion rate setting when device is in continuous conversion mode.
                         // 00: 27.5 ms conversion rate
                         // 01: 55 ms conversion rate
                         // 10: 110 ms conversion rate
                         // 11: 220 ms conversion rate (35 ms TMP1075N)

      uint8_t faults : 2;  // Consecutive fault measurements to trigger the alert function.
                           // 00: 1 fault
                           // 01: 2 faults
                           // 10: 3 faults (4 faults TMP1075N)
                           // 11: 4 faults (6 faults TMP1075N)

      uint8_t polarity : 1;  // Polarity of the output pin.
                             // 0: Active low ALERT pin
                             // 1: Active high ALERT pin

      uint8_t alert_mode : 1;  // Selects the function of the ALERT pin.
                               // 0: ALERT pin functions in comparator mode
                               // 1: ALERT pin functions in interrupt mode

      uint8_t shutdown : 1;  // Sets the device in shutdown mode to conserve power.
                             // 0: Device is in continuous conversion
                             // 1: Device is in shutdown mode
      uint8_t unused : 8;
    } fields;
    uint16_t regvalue;
  };
};

enum EConversionRate {
  CONV_RATE_27_5_MS,
  CONV_RATE_55_MS,
  CONV_RATE_110_MS,
  CONV_RATE_220_MS,
};

enum EAlertFunction {
  ALERT_COMPARATOR = 0,
  ALERT_INTERRUPT = 1,
};

class TMP1075Sensor : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  // Call write_config() after calling any of these to send the new config to
  // the IC. The setup() function also does this.
  void set_alert_limit_low(const float temp) { this->alert_limit_low_ = temp; }
  void set_alert_limit_high(const float temp) { this->alert_limit_high_ = temp; }
  void set_oneshot(const bool oneshot) { config_.fields.oneshot = oneshot; }
  void set_conversion_rate(const enum EConversionRate rate) { config_.fields.rate = rate; }
  void set_alert_polarity(const bool polarity) { config_.fields.polarity = polarity; }
  void set_alert_function(const enum EAlertFunction function) { config_.fields.alert_mode = function; }
  void set_fault_count(int faults);

  void write_config();

 protected:
  TMP1075Config config_ = {};

  // Disable the alert pin by default.
  float alert_limit_low_ = -128.0f;
  float alert_limit_high_ = 127.9375f;

  void send_alert_limit_low_();
  void send_alert_limit_high_();
  void send_config_();
  void log_config_();
};

}  // namespace tmp1075
}  // namespace esphome
