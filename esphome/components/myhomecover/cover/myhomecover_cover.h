#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace myhomecover {

class MyHomeCover : public cover::Cover, public Component{
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_channel(std::string channel);
  void set_ip(std::string ip){ this->ip_ = ip;};
  void set_auth(bool doesit, int password);
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  bool is_at_target_() const;
  void send_command_(std::string canal, int command);
  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();
  std::string channel_;
  uint32_t open_duration_;
  uint32_t close_duration_;
  std::string ip_;

  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  bool needs_auth_ = false;
  int password_ = 12345;
  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
};

}  // namespace myhomecover
}  // namespace esphome
