#pragma once

#include <algorithm>
#include <vector>

#include "esphome/components/canopen/canopen.h"

namespace esphome::canopen::testing {

class MockCANbus : public canbus::Canbus {
 public:
  std::vector<canbus::CanFrame> sent;
  canbus::Error result{canbus::ERROR_OK};

 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override {
    this->sent.push_back(*frame);
    return this->result;
  }
  canbus::Error read_message(canbus::CanFrame *) override { return canbus::ERROR_NOMSG; }
};

class ProbeClient : public CANopenComponent {
 public:
  explicit ProbeClient(canbus::Canbus *bus) : CANopenComponent(bus) { this->setup(); }
  void on_frame_(uint32_t id, bool extended, bool rtr, const std::vector<uint8_t> &data) {
    canbus::CanFrame frame{};
    frame.can_id = id;
    frame.can_data_length_code = data.size();
    std::copy(data.begin(), data.end(), frame.data);
    frame.use_extended_id = extended;
    frame.remote_transmission_request = rtr;
    CANopenComponent::on_frame_(frame);
  }
  void expire() { this->last_ms_ -= this->response_timeout_ms_ + 1; }
};

}  // namespace esphome::canopen::testing
