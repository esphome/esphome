#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace hdmi_cec {

enum class DecoderState : uint8_t {
  Idle = 0,
  StartBitReceived = 1,
  ReadingEightBits = 2,
  WaitingForEOM = 3,
  WaitingForAck = 4,
};

class HDMICEC : public Component {
public:
  void set_cec_pin(InternalGPIOPin *cec_pin) { cec_pin_ = cec_pin; };

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() {};

  static void falling_edge_interrupt(HDMICEC *self);
  static void rising_edge_interrupt(HDMICEC *self);

protected:
  InternalGPIOPin *cec_pin_;
  uint32_t last_falling_edge_ms_;
  DecoderState decoder_state_;
  uint8_t byte_recv_buffer_;
  std::vector<uint8_t> frame_recv_buffer_;
};

}
}
