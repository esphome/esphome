#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace somfycover {

class SomfyCover : public cover::Cover, public Component{
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_remote_id(uint32_t remote_id);
  void set_rolling_code_(globals::RestoringGlobalsComponent<int>* rollingcode){
    this->rollingCodeStorage_ = rollingcode;
  }
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  bool is_at_target_() const;
  //void send_command_(std::string canal, int command);
  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();
  uint32_t remote_id_;
  uint32_t open_duration_;
  uint32_t close_duration_;

  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
  globals::RestoringGlobalsComponent<int> *rollingCodeStorage_;
};

}  // namespace somfycover
}  // namespace esphome
