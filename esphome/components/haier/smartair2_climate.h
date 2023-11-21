#pragma once

#include <chrono>
#include "haier_base.h"

namespace esphome {
namespace haier {

class Smartair2Climate : public HaierClimateBase {
 public:
  Smartair2Climate();
  Smartair2Climate(const Smartair2Climate &) = delete;
  Smartair2Climate &operator=(const Smartair2Climate &) = delete;
  ~Smartair2Climate();
  void dump_config() override;
  void set_alternative_swing_control(bool swing_control);

 protected:
  void set_handlers() override;
  void process_phase(std::chrono::steady_clock::time_point now) override;
  haier_protocol::HaierMessage get_power_message(bool state) override;
  haier_protocol::HaierMessage get_control_message() override;
  // Answer handlers
  haier_protocol::HandlerError status_handler_(haier_protocol::FrameType request_type,
                                               haier_protocol::FrameType message_type, const uint8_t *data,
                                               size_t data_size);
  haier_protocol::HandlerError get_device_version_answer_handler_(haier_protocol::FrameType request_type,
                                                                  haier_protocol::FrameType message_type,
                                                                  const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_device_id_answer_handler_(haier_protocol::FrameType request_type,
                                                             haier_protocol::FrameType message_type,
                                                             const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError messages_timeout_handler_with_cycle_for_init_(haier_protocol::FrameType message_type);
  // Helper functions
  haier_protocol::HandlerError process_status_message_(const uint8_t *packet, uint8_t size);
  bool use_alternative_swing_control_;
};

}  // namespace haier
}  // namespace esphome
