#include "rs485_frame_button.h"

namespace esphome::rs485_frame {

void RS485FrameButton::press_action() {
  if (this->raw_mode_) {
    this->parent_->queue_raw_frame(this->raw_frame_);
  } else if (this->has_cmd_format_) {
    std::vector<uint8_t> preamble(this->cmd_preamble_.begin(), this->cmd_preamble_.end());
    std::vector<uint8_t> postamble(this->cmd_postamble_.begin(), this->cmd_postamble_.end());
    this->parent_->queue_command_with_format(this->command_values_.data(), this->command_values_.size(), preamble,
                                             this->cmd_value_element_bytes_, this->cmd_big_endian_, postamble);
  } else if (this->has_value_element_bytes_override_) {
    this->parent_->queue_command_values_with_element_bytes(this->command_values_.data(), this->command_values_.size(),
                                                           this->value_element_bytes_override_);
  } else {
    this->parent_->queue_command_values(this->command_values_.data(), this->command_values_.size());
  }
}

}  // namespace esphome::rs485_frame
