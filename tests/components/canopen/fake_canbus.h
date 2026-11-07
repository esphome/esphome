#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/canopen/canopen.h"

namespace esphome::canopen::testing {

// records the frames the hub sends
class FakeCanbus : public canbus::Canbus {
 public:
  struct Frame {
    uint32_t can_id;
    std::vector<uint8_t> data;
  };
  std::vector<Frame> sent;
  // the platform refuses frames (TX buffers full)
  bool busy{false};

  size_t count(uint32_t can_id) const {
    size_t n = 0;
    for (const auto &f : this->sent)
      n += f.can_id == can_id;
    return n;
  }
  const Frame *find(uint32_t can_id) const {
    for (const auto &f : this->sent) {
      if (f.can_id == can_id)
        return &f;
    }
    return nullptr;
  }

 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override {
    if (this->busy)
      return canbus::ERROR_ALLTXBUSY;
    this->sent.push_back({frame->can_id, {frame->data, frame->data + frame->can_data_length_code}});
    return canbus::ERROR_OK;
  }
  canbus::Error read_message(canbus::CanFrame *frame) override { return canbus::ERROR_NOMSG; }
};

class TestHub : public CANopenComponent {
 public:
  // a received frame, as the canbus callback delivers it (processed right away)
  void frame(uint32_t can_id, const std::vector<uint8_t> &data) {
    canbus::CanFrame f{};
    f.can_id = can_id;
    f.can_data_length_code = data.size();
    std::copy(data.begin(), data.end(), f.data);
    this->on_frame_(f);
  }
};

}  // namespace esphome::canopen::testing
