#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace tuya {

class TuyaFan : public Component, public fan::Fan {
 public:
  TuyaFan(Tuya *parent, int speed_count) : parent_(parent), speed_count_(speed_count) {}
  void setup() override;
  void dump_config() override;
  void set_speed_id(uint8_t speed_id) { this->speed_id_ = speed_id; }
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }
  void set_oscillation_id(uint8_t oscillation_id) { this->oscillation_id_ = oscillation_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void set_preset_modes_id(uint8_t preset_modes_id) { this->preset_modes_id_ = preset_modes_id; }
  void set_preset_modes(std::initializer_list<const char *> presets) { this->preset_modes_ = presets; }

  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;

  Tuya *parent_;
  optional<uint8_t> speed_id_{};
  optional<uint8_t> switch_id_{};
  optional<uint8_t> oscillation_id_{};
  optional<uint8_t> direction_id_{};
  optional<uint8_t> preset_modes_id_{};
  std::vector<const char *> preset_modes_{};
  fan::FanTraits traits_;
  int speed_count_{};
  TuyaDatapointType speed_type_{};
  TuyaDatapointType oscillation_type_{};
  TuyaDatapointType preset_modes_type_{};
};

}  // namespace tuya
}  // namespace esphome
