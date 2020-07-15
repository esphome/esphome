#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace tuya {

class TuyaClimate : public climate::Climate, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_target_temperature_id(uint8_t target_temperature_id) {
    this->target_temperature_id_ = target_temperature_id;
  }
  void set_current_temperature_id(uint8_t current_temperature_id) {
    this->current_temperature_id_ = current_temperature_id;
  }
  // void set_eco_mode_id(uint8_t eco_mode_id) { this->eco_mode_id_ = eco_mode_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Re-compute the state of this climate controller.
  void compute_state_();

  /// Switch the climate device to the given climate mode.
  void switch_to_action_(climate::ClimateAction action);

  Tuya *parent_;
  optional<uint8_t> switch_id_{};
  optional<uint8_t> target_temperature_id_{};
  optional<uint8_t> current_temperature_id_{};
  // optional<uint8_t> eco_mode_id_{};
};

}  // namespace tuya
}  // namespace esphome
