#pragma once

#include <vector>
#include <queue>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace hdmi_cec {

enum class ReceiverState : uint8_t {
  Idle = 0,
  ReceivingByte = 2,
  WaitingForEOM = 3,
  WaitingForAck = 4,
  WaitingForEOMAck = 5,
};

class MessageTrigger;

class HDMICEC : public Component {
public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_address(uint8_t address) { address_ = address; }
  uint8_t address() { return address_; }
  void set_promiscuous_mode(bool promiscuous_mode) { promiscuous_mode_ = promiscuous_mode; }
  void add_message_trigger(MessageTrigger *trigger) { message_triggers_.push_back(trigger); }

  bool send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes);

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

protected:
  static void gpio_intr(HDMICEC *self);
  static void reset_state_variables_(HDMICEC *self);
  bool send_frame_(const std::vector<uint8_t> &frame, bool is_broadcast);
  void send_start_bit_();
  void send_bit_(bool bit_value);
  bool send_and_read_ack_(bool is_broadcast);
  void switch_to_listen_mode_();
  void switch_to_send_mode_();

  InternalGPIOPin *pin_;
  ISRInternalGPIOPin isr_pin_;
  uint8_t address_;
  bool promiscuous_mode_;
  std::vector<MessageTrigger*> message_triggers_;

  uint32_t last_falling_edge_us_;
  ReceiverState receiver_state_;
  uint8_t recv_bit_counter_;
  uint8_t recv_byte_buffer_;
  std::vector<uint8_t> recv_frame_buffer_;
  std::queue<std::vector<uint8_t>> recv_queue_;
  bool recv_ack_queued_;
  Mutex send_mutex_;
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

template<typename... Ts> class SendAction : public Action<Ts...> {
public:
  SendAction(HDMICEC *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void set_source(uint8_t source) { source_ = source; }
  void set_destination(uint8_t destination) { destination_ = destination; }
   
  void play(Ts... x) override {
    auto source_address = source_.has_value() ? source_.value() : parent_->address();
    auto data = data_.value(x...);
    parent_->send(source_address, destination_, data);
  }

protected:
  HDMICEC *parent_;
  optional<uint8_t> source_;
  uint8_t destination_;
};

}
}
