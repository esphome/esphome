#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace somfyrts {

class SomfyRTSCover : public cover::Cover, public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_channel(uint8_t channel);
  void set_node_id(uint8_t nodeid_1, uint8_t nodeid_2, uint8_t nodeid_3);
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  bool is_at_target_() const;

  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();
  void calc_chksum(uint8_t *frame, uint8_t frame_size);
  uint8_t node_id_1_;
  uint8_t node_id_2_;
  uint8_t node_id_3_;
  uint8_t channel_;
  uint32_t open_duration_;
  uint32_t close_duration_;

  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
};

}  // namespace somfyrts
}  // namespace esphome
