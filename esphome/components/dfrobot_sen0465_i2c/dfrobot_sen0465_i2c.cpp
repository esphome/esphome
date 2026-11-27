#include "dfrobot_sen0465_i2c.h"
#include "esphome/core/log.h"

namespace esphome::dfrobot_sen0465_i2c {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0465_i2c");

using dfrobot_sen0465_base::FRAME_SIZE;
using dfrobot_sen0465_base::RESPONSE_TIMEOUT_ID;

static constexpr uint8_t DATA_REGISTER = 0x00;
// The DFRobot library waits 10 ms, but on an ESP32-C6 reading the answer to the alarm command
// 10 ms after sending it timed out
static constexpr uint32_t RESPONSE_DELAY_MS = 100;

void DFRobotSEN0465I2CComponent::dump_config() {
  DFRobotSEN0465Component::dump_config();
  LOG_I2C_DEVICE(this);
}

bool DFRobotSEN0465I2CComponent::write_frame(const uint8_t *frame) {
  return this->write_register(DATA_REGISTER, frame, FRAME_SIZE) == i2c::ERROR_OK;
}

void DFRobotSEN0465I2CComponent::read_response() {
  this->set_timeout(RESPONSE_TIMEOUT_ID, RESPONSE_DELAY_MS, [this]() {
    uint8_t frame[FRAME_SIZE];
    // Like the DFRobot library, end the register write with a STOP before reading
    if (this->write(&DATA_REGISTER, 1) != i2c::ERROR_OK || this->read(frame, FRAME_SIZE) != i2c::ERROR_OK) {
      this->handle_no_response_();
      return;
    }
    this->handle_response_(frame);
  });
}

}  // namespace esphome::dfrobot_sen0465_i2c
