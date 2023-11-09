#pragma once

#include <vector>
#include <queue>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace hdmi_cec {

enum class DecoderState : uint8_t {
  Idle = 0,
  ReceivingByte = 2,
  WaitingForEOM = 3,
  WaitingForAck = 4,
  WaitingForEOMAck = 5,
};

class MessageTrigger;

class HDMICEC : public Component {
public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; };
  void set_address(uint8_t address) { address_ = address; };
  void set_promiscuous_mode(bool promiscuous_mode) { promiscuous_mode_ = promiscuous_mode; };
  void add_message_trigger(MessageTrigger *trigger) { message_triggers_.push_back(trigger); };

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

  static void gpio_intr(HDMICEC *self);

protected:
  static void reset_state_variables(HDMICEC *self);

  InternalGPIOPin *pin_;
  ISRInternalGPIOPin isr_pin_;
  uint8_t address_;
  bool promiscuous_mode_;
  std::vector<MessageTrigger*> message_triggers_;

  uint32_t last_falling_edge_us_;
  DecoderState decoder_state_;
  uint8_t recv_bit_counter_;
  uint8_t recv_byte_buffer_;
  std::vector<uint8_t> recv_frame_buffer_;
  std::queue<std::vector<uint8_t>> recv_queue_;
};

class MessageTrigger : public Trigger<uint8_t, uint8_t, std::vector<uint8_t>> {
  friend class HDMICEC;

public:
  explicit MessageTrigger(HDMICEC *parent) { parent->add_message_trigger(this); };
  void set_source(uint8_t source) { source_ = source; };
  void set_destination(uint8_t destination) { destination_ = destination; };
  void set_opcode(uint8_t opcode) { opcode_ = opcode; };
  void set_data(const std::vector<uint8_t> &data) { data_ = data; };

protected:
  optional<uint8_t> source_;
  optional<uint8_t> destination_;
  optional<uint8_t> opcode_;
  optional<std::vector<uint8_t>> data_;
};

}
}
