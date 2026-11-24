#pragma once

#include <algorithm>
#include <deque>
#include <initializer_list>
#include <vector>

#include "esphome/components/canbus/canbus.h"

namespace esphome::canbus::testing {

inline std::vector<uint8_t> payload_bytes(const CanFrame &frame) { return frame.payload(); }

// Hands queued frames to the bus and records the frames it sends
class QueuedCanbus : public Canbus {
 public:
  void queue(uint32_t can_id, bool extended, std::initializer_list<uint8_t> data) {
    CanFrame frame{};
    frame.can_id = can_id;
    frame.use_extended_id = extended;
    frame.can_data_length_code = static_cast<uint8_t>(data.size());
    std::copy(data.begin(), data.end(), frame.data);
    this->rx_.push_back(frame);
  }
  void queue(const CanFrame &frame) { this->rx_.push_back(frame); }

  std::vector<CanFrame> sent;
  Error send_result{ERROR_OK};

 protected:
  bool setup_internal() override { return true; }
  Error send_message(CanFrame *frame) override {
    this->sent.push_back(*frame);
    return this->send_result;
  }
  Error read_message(CanFrame *frame) override {
    if (this->rx_.empty())
      return ERROR_NOMSG;
    *frame = this->rx_.front();
    this->rx_.pop_front();
    return ERROR_OK;
  }

  std::deque<CanFrame> rx_;
};

}  // namespace esphome::canbus::testing
