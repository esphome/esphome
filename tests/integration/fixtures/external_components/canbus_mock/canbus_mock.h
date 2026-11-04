#pragma once

// HOST-ONLY TEST COMPONENT: every sent frame is queued and read back as a received frame.

#include "esphome/components/canbus/canbus.h"

#include <array>

namespace esphome::canbus_mock {

class MockCanbus : public canbus::Canbus {
 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override;
  canbus::Error read_message(canbus::CanFrame *frame) override;

  static constexpr size_t QUEUE_SIZE = 16;
  std::array<canbus::CanFrame, QUEUE_SIZE> queue_{};
  size_t head_{0};
  size_t count_{0};
};

}  // namespace esphome::canbus_mock
