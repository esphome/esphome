#include "dfrobot_sen0465_uart.h"

#include <algorithm>

namespace esphome::dfrobot_sen0465_uart {

using dfrobot_sen0465_base::FRAME_HEAD;
using dfrobot_sen0465_base::FRAME_SIZE;
using dfrobot_sen0465_base::RESPONSE_TIMEOUT_ID;

// The DFRobot library waits up to this long for an answer
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 3000;

void DFRobotSEN0465UARTComponent::setup() {
  // loop() only runs while an answer is expected
  this->disable_loop();
  DFRobotSEN0465Component::setup();
}

void DFRobotSEN0465UARTComponent::loop() {
  // The tail of a reading the probe sent on its own may precede the answer; start at the next frame head.
  uint8_t byte;
  while (this->rx_count_ == 0 && this->peek_byte(&byte) && byte != FRAME_HEAD)
    this->read_byte(&byte);
  const size_t available = this->available();
  if (available == 0)
    return;
  const size_t len = std::min<size_t>(available, FRAME_SIZE - this->rx_count_);
  if (!this->read_array(this->rx_buffer_ + this->rx_count_, len))
    return;
  this->rx_count_ += len;
  if (this->rx_count_ < FRAME_SIZE)
    return;

  this->rx_count_ = 0;
  this->cancel_timeout(RESPONSE_TIMEOUT_ID);
  this->disable_loop();
  this->handle_response_(this->rx_buffer_);
}

bool DFRobotSEN0465UARTComponent::write_frame(const uint8_t *frame) {
  // Drop anything left over, such as the answer to the mode change or readings sent before passive mode was set
  this->discard_available();
  this->rx_count_ = 0;
  this->write_array(frame, FRAME_SIZE);
  return true;
}

void DFRobotSEN0465UARTComponent::read_response() {
  this->enable_loop();
  this->set_timeout(RESPONSE_TIMEOUT_ID, RESPONSE_TIMEOUT_MS, [this]() {
    this->disable_loop();
    this->rx_count_ = 0;
    this->handle_no_response_();
  });
}

}  // namespace esphome::dfrobot_sen0465_uart
