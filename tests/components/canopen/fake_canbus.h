#pragma once

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "esphome/components/canopen/canopen.h"
#include "esphome/core/application.h"

namespace esphome::canopen::testing {

inline canbus::CanFrame make_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
  canbus::CanFrame f{};
  f.can_id = can_id;
  f.can_data_length_code = data.size();
  std::copy(data.begin(), data.end(), f.data);
  return f;
}

// records the frames the hub sends
class FakeCanbus : public canbus::Canbus {
 public:
  struct Frame {
    uint32_t can_id;
    std::vector<uint8_t> data;
  };
  std::vector<Frame> sent;
  // refuse frames, as with full TX buffers
  bool busy{false};
  // runs inside send_message(), before the frame is taken or refused (as a driver's log handler would)
  std::function<void(uint32_t can_id)> on_send;
  // received frames, handed out by loop()
  std::deque<canbus::CanFrame> rx;

  void queue(uint32_t can_id, const std::vector<uint8_t> &data) { this->rx.push_back(make_frame(can_id, data)); }

  size_t count(uint32_t can_id) const {
    size_t n = 0;
    for (const auto &f : this->sent)
      n += f.can_id == can_id;
    return n;
  }

 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override {
    if (this->on_send) {
      auto hook = this->on_send;  // the hook may clear itself
      hook(frame->can_id);
    }
    if (this->busy)
      return canbus::ERROR_ALLTXBUSY;
    this->sent.push_back({frame->can_id, {frame->data, frame->data + frame->can_data_length_code}});
    return canbus::ERROR_OK;
  }
  canbus::Error read_message(canbus::CanFrame *frame) override {
    if (this->rx.empty())
      return canbus::ERROR_NOMSG;
    *frame = this->rx.front();
    this->rx.pop_front();
    return canbus::ERROR_OK;
  }
};

class TestHub : public CANopenComponent {
 public:
  // frames queued in the CANopenNode driver, waiting to be sent
  uint16_t tx_count() const { return this->co_->CANmodule->CANtxCount; }
  using CANopenComponent::CANopenComponent;
  // delivers a frame as the canbus callback does
  void frame(uint32_t can_id, const std::vector<uint8_t> &data) { this->on_frame_(make_frame(can_id, data)); }
  void frame(const canbus::CanFrame &f) { this->on_frame_(f); }
};

// Requests to node 0x21 go out on 0x621, its replies come on 0x5A1.
class SdoTest : public ::testing::Test {
 protected:
  FakeCanbus bus_;
  TestHub hub_{&this->bus_};
  std::vector<SDOReply> replies_;
  std::string data_;  // reply.data, copied during the callback
  uint16_t response_timeout_ms_{20};

  void SetUp() override {
    this->hub_.set_response_timeout(this->response_timeout_ms_);
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
  }

  static void on_reply(void *context, const SDOReply &reply) {
    auto *self = static_cast<SdoTest *>(context);
    self->replies_.push_back(reply);
    if (reply.data != nullptr)
      self->data_.assign(reinterpret_cast<const char *>(reply.data), reply.len);
  }

  std::vector<std::vector<uint8_t>> requests_(uint32_t can_id = 0x621) const {
    std::vector<std::vector<uint8_t>> out;
    for (const auto &f : this->bus_.sent) {
      if (f.can_id == can_id)
        out.push_back(f.data);
    }
    return out;
  }

  // advances the loop clock
  static void advance_(uint32_t ms) {
    LoopBlockingGuard clock{nullptr, nullptr, App.get_loop_component_start_time() + ms};
  }

  // long enough for the response timeout to end a transfer
  void wait_for_timeout_() {
    for (int i = 0; i < 60; i++) {
      this->hub_.loop();
      advance_(1);
    }
  }
};

}  // namespace esphome::canopen::testing
