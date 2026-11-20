#pragma once

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
  using CANopenComponent::CANopenComponent;
  using CANopenComponent::on_frame_;
  void expire() { this->sent_at_ms_ -= this->response_timeout_ms_; }
};

}  // namespace esphome::canopen::testing
