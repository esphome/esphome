#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace tuya {

class TuyaLight : public Component, public light::LightOutput {
 public:
  void setup() override;
  void dump_config() override;
  void set_dimmer_id(uint8_t dimmer_id) { this->dimmer_id_ = dimmer_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { max_value_ = max_value; }
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  void update_dimmer_(uint32_t value);
  void update_switch_(uint32_t value);

  Tuya *parent_;
  optional<uint8_t> dimmer_id_{};
  optional<uint8_t> switch_id_{};
  uint32_t min_value_ = 0;
  uint32_t max_value_ = 255;
  light::LightState *state_{nullptr};
};

}  // namespace tuya
}  // namespace esphome
