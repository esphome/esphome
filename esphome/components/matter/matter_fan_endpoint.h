#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_FAN

#include <cstdint>

namespace esphome {
namespace fan {
class Fan;
}
}  // namespace esphome

namespace esphome {
namespace matter {

// Wraps one ESPHome fan as a Matter fan device (FanControl cluster).
//
// Attribute mapping:
//   FanMode (0x0000, enum8, writable) 0=Off 1=Low 2=Med 3=High 4=On 5=Auto
//   PercentSetting (0x0002, nullable uint8, writable) 0..100 — fabric-driven
//   PercentCurrent (0x0003, uint8) 0..100 — device-reported
//
// ESPHome exposes `state` (bool), `speed` (1..supported_speed_count), and a
// `void()` state callback. Speed↔percent conversion is linear over the
// supported_speed_count range. Speed=0 is off (state=false).
//
// Fabric → device (writes to FanMode or PercentSetting land in the
// MatterComponent dispatcher which forwards to on_matter_percent_write /
// on_matter_fan_mode_write): translate to make_call().set_state()/set_speed()
// and perform().
//
// Device → fabric (fan state callback fires with no args): re-read the fan's
// state + speed, translate to PercentCurrent + FanMode, update() the fabric.
//
// Two guards prevent loops in the same shape as MatterSwitchEndpoint and
// MatterCoverEndpoint: applying_matter_write_ suppresses the device→fabric
// echo while we drive make_call(); applying_report_ suppresses the fabric→
// device dispatch when attribute::update() re-enters via PRE_UPDATE.
class MatterFanEndpoint {
 public:
  explicit MatterFanEndpoint(fan::Fan *fan);

  bool setup();

  // Called by the dispatcher when the fabric writes PercentSetting (uint8 0..100).
  void on_matter_percent_write(uint8_t percent);
  // Called by the dispatcher when the fabric writes FanMode (uint8 enum).
  void on_matter_fan_mode_write(uint8_t fan_mode);

  void push_initial_state();

  uint16_t endpoint_id() const { return endpoint_id_; }
  fan::Fan *esphome_fan() const { return fan_; }
  bool applying_report() const { return applying_report_; }

 protected:
  void report_state_to_fabric_();
  uint8_t esphome_to_percent_() const;
  uint8_t percent_to_speed_(uint8_t percent) const;
  uint8_t percent_to_fan_mode_(uint8_t percent) const;

  fan::Fan *fan_;
  uint16_t endpoint_id_{0};
  int supported_speed_count_{1};
  bool supports_speed_{false};
  bool applying_matter_write_{false};
  bool applying_report_{false};
};

}  // namespace matter
}  // namespace esphome

#endif  // USE_FAN
#endif  // USE_ESP_IDF
