#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace tuya {

enum TuyaCoverRestoreMode {
  COVER_NO_RESTORE,
  COVER_RESTORE,
  COVER_RESTORE_AND_CALL,
};

class TuyaCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_control_id(uint8_t control_id) { this->control_id_ = control_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void set_position_id(uint8_t position_id) { this->position_id_ = position_id; }
  void set_position_report_id(uint8_t position_report_id) { this->position_report_id_ = position_report_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { max_value_ = max_value; }
  void set_invert_position(bool invert_position) { invert_position_ = invert_position; }
  void set_invert_position_report(bool invert_position_report) { invert_position_report_ = invert_position_report; }
  void set_restore_mode(TuyaCoverRestoreMode restore_mode) { restore_mode_ = restore_mode; }

 protected:
  void control(const cover::CoverCall &call) override;
  void set_direction_(bool inverted);
  cover::CoverTraits get_traits() override;

  Tuya *parent_;
  TuyaCoverRestoreMode restore_mode_{};
  optional<uint8_t> control_id_{};
  optional<uint8_t> direction_id_{};
  optional<uint8_t> position_id_{};
  optional<uint8_t> position_report_id_{};
  uint32_t min_value_;
  uint32_t max_value_;
  uint32_t value_range_;
  bool invert_position_;
  bool invert_position_report_;
};

}  // namespace tuya
}  // namespace esphome
