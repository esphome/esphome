#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace tuya {

class TuyaCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_position_id(uint8_t dimmer_id) { this->position_id_ = dimmer_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { max_value_ = max_value; }
  void set_invert_position(bool invert_position) { invert_position_ = invert_position; }

 protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;

  Tuya *parent_;
  optional<uint8_t> position_id_{};
  uint32_t min_value_;
  uint32_t max_value_;
  uint32_t value_range_;
  bool invert_position_;
};

}  // namespace tuya
}  // namespace esphome
