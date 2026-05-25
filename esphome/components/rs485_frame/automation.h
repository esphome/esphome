#pragma once

#include "rs485_frame.h"
#include "esphome/core/automation.h"

#include <vector>

namespace esphome::rs485_frame {

/// Action: queue a raw frame for transmission. The hub adds DLE-STX/ETX wrapping,
/// applies byte-stuffing, and computes the CRC according to the configured
/// crc.type and crc.tx_variant. The caller supplies the 2-byte (or longer)
/// frame_type and the unframed, unstuffed payload bytes; the action concatenates
/// them before handing to RS485FrameHub::queue_raw_frame.
///
/// Both frame_type and payload are templatable so they can be computed from
/// trigger arguments or other entity states at action time.
template<typename... Ts> class SendFrameAction : public Action<Ts...>, public Parented<RS485FrameHub> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, frame_type)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, payload)

  void play(Ts... x) override {
    // Hand the two byte lists to the hub, which assembles them into a pre-reserved buffer
    // and enforces max_frame_length — no per-call assembly vector here.
    this->parent_->queue_raw_frame(this->frame_type_.value(x...), this->payload_.value(x...));
  }
};

}  // namespace esphome::rs485_frame
