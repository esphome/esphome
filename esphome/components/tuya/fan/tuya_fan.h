#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace tuya {

class TuyaFan : public Component {
 public:
  TuyaFan(Tuya *parent, fan::FanState *fan, int speed_count) : parent_(parent), fan_(fan), speed_count_(speed_count) {}
  void setup() override;
  float get_setup_priority() const override;
  void dump_config() override;
  void set_speed_id(uint8_t speed_id) { this->speed_id_ = speed_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_oscillation_id(uint8_t oscillation_id) { this->oscillation_id_ = oscillation_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void write_state();

 protected:
  void update_speed_(uint32_t value);
  void update_switch_(uint32_t value);
  void update_oscillation_(uint32_t value);
  void update_direction_(uint32_t value);

  Tuya *parent_;
  optional<uint8_t> speed_id_{};
  optional<uint8_t> switch_id_{};
  optional<uint8_t> oscillation_id_{};
  optional<uint8_t> direction_id_{};
  fan::FanState *fan_;
  int speed_count_{};
};

}  // namespace tuya
}  // namespace esphome
