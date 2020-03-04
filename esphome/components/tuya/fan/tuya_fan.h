#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace tuya {

class TuyaFan : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_speed_id(uint8_t speed_id) { this->speed_id_ = speed_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_oscillation_id(uint8_t oscillation_id) { this->oscillation_id_ = oscillation_id; }
  void set_fan(fan::FanState *fan) { this->fan_ = fan; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void write_state();

 protected:
  void update_speed_(uint32_t value);
  void update_switch_(uint32_t value);
  void update_oscillation_(uint32_t value);

  Tuya *parent_;
  optional<uint8_t> speed_id_{};
  optional<uint8_t> switch_id_{};
  optional<uint8_t> oscillation_id_{};
  fan::FanState *fan_;
};

}  // namespace tuya
}  // namespace esphome
