#include "rs485_frame_number.h"

namespace esphome::rs485_frame {

void RS485FrameNumber::control(float value) {
  if (this->lambda_ == nullptr)
    return;
  auto payload = this->lambda_(value);
  if (!payload.has_value())
    return;
  if (this->parent_->queue_frame(payload->data(), payload->size()))
    this->publish_state(value);
}

}  // namespace esphome::rs485_frame
