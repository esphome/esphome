#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace he60r {

class HE60rCover : public cover::Cover, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void set_open_duration(uint32_t duration) { this->open_duration_ = duration; }
  void set_close_duration(uint32_t duration) { this->close_duration_ = duration; }

  cover::CoverTraits get_traits() override;

 protected:
  void update_();
  void control(const cover::CoverCall &call) override;
  bool is_at_target_() const;
  void start_direction_(cover::CoverOperation dir);
  void endstop_reached_(cover::CoverOperation operation);
  void recompute_position_();
  void set_current_operation_(cover::CoverOperation operation);
  void process_rx_(uint8_t data);

  unsigned open_duration_{0};
  unsigned close_duration_{0};
  unsigned toggles_needed_{0};
  cover::CoverOperation next_direction_{cover::COVER_OPERATION_IDLE};
  cover::CoverOperation last_command_{cover::COVER_OPERATION_IDLE};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  float target_position_{0};
  bool query_seen_{};
  uint8_t counter_{};
};

}  // namespace he60r
}  // namespace esphome
