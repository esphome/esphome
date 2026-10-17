#include "rs485_frame_number.h"

namespace esphome::rs485_frame {

// control() is a user-initiated path (a slider/number set from HA or an automation), not a
// hot loop, so the std::vector the lambda returns is an acceptable per-action allocation --
// the same trade-off the templatable send_frame action makes. The platform exists so a
// number entity can map a scalar to an encoded frame (e.g. pump speed -> command bytes)
// without the user writing a button per value; queue_raw_frame still enforces the length
// bound and no-heap-after-setup on the actual TX buffers.
void RS485FrameNumber::control(float value) {
  if (this->lambda_ == nullptr)
    return;
  auto payload = this->lambda_(value);
  if (!payload.has_value())
    return;
  if (this->parent_->queue_raw_frame(payload.value()))
    this->publish_state(value);
}

}  // namespace esphome::rs485_frame
