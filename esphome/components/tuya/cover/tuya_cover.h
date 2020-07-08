#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace tuya {

enum TuyaCoverRestoreMode {
  COVER_NO_RESTORE,
  COVER_RESTORE,
  COVER_RESTORE_AND_CALL,
};

class TuyaCover : public cover::Cover, public Component, api::CustomAPIDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_optimistic(bool optimistic);
  void set_direction(bool inverted);
  void set_position_report_id(uint8_t position_report_id) { this->position_report_id_ = position_report_id; }
  void set_position_control_id(uint8_t position_control_id) { this->position_control_id_ = position_control_id; }
  void set_control_id(uint8_t control_id) { this->control_id_ = control_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void set_restore_mode(TuyaCoverRestoreMode restore_mode) { restore_mode_ = restore_mode; }

 protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;

  Tuya *parent_;
  TuyaCoverRestoreMode restore_mode_{COVER_RESTORE};
  bool optimistic_{false};
  optional<uint8_t> position_report_id_{};
  optional<uint8_t> position_control_id_{};
  optional<uint8_t> control_id_{};
  optional<uint8_t> direction_id_{};
};

}  // namespace tuya
}  // namespace esphome
