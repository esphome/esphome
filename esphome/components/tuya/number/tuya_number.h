#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace tuya {

class TuyaNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_number_id(uint8_t number_id) { this->number_id_ = number_id; }
  void set_write_multiply(float factor) { multiply_by_ = factor; }
  void set_datapoint_type(TuyaDatapointType type) { type_ = type; }
  void set_datapoint_initial_value(float value) { this->initial_value_ = value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;

  Tuya *parent_;
  uint8_t number_id_{0};
  float multiply_by_{1.0};
  optional<TuyaDatapointType> type_{};
  optional<float> initial_value_{};
  bool restore_value_{false};

  ESPPreferenceObject pref_;
};

}  // namespace tuya
}  // namespace esphome
