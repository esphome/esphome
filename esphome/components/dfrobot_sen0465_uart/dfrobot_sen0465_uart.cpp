#include "dfrobot_sen0465_uart.h"

namespace esphome::dfrobot_sen0465_uart {

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
  // The tail of a reading the probe sent on its own may precede the answer; the reader skips to the frame head.
  const uint8_t *frame = nullptr;
  uint8_t byte;
  while (frame == nullptr && this->available() > 0 && this->read_byte(&byte))
    frame = this->reader_.feed(byte);
  if (frame == nullptr)
    return;

  this->cancel_timeout(RESPONSE_TIMEOUT_ID);
  this->disable_loop();
  this->handle_response_(frame);
}

bool DFRobotSEN0465UARTComponent::write_frame(const uint8_t *frame) {
  // Drop anything left over, such as the answer to the mode change or readings sent before passive mode was set
  this->discard_available();
  this->reader_.reset();
  this->write_array(frame, FRAME_SIZE);
  return true;
}

void DFRobotSEN0465UARTComponent::read_response() {
  this->enable_loop();
  this->set_timeout(RESPONSE_TIMEOUT_ID, RESPONSE_TIMEOUT_MS, [this]() {
    this->disable_loop();
    this->reader_.reset();
    this->handle_no_response_();
  });
}

}  // namespace esphome::dfrobot_sen0465_uart
