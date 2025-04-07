#pragma once

#include <vector>
#include <queue>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace uart {
  class UARTComponent;
}
namespace hdmi_cec {

enum class ReceiverState : uint8_t {
  Idle = 0,
  ReceivingByte = 2,
  WaitingForEOM = 3,
  WaitingForAck = 4,
  WaitingForEOMAck = 5,
};

enum class TransmitState : uint8_t {
  Idle         = 0,
  Busy         = 1,
  EomConfirmed = 2,
};

class MessageTrigger;

class Message : public std::vector<uint8_t> {
public:
  Message() = default;
  Message(uint8_t initiator_addr, uint8_t target_addr, const std::vector<uint8_t> &payload);
  uint8_t initiator_addr() const { return (this->at(0) >> 4) & 0xf; }
  uint8_t destination_addr() const { return this->at(0) & 0xf; }
  bool is_broadcast() const { return this->destination_addr() == 0xf; }
  std::string to_string() const;
};

class CECTransmit {
public:
  void setup(InternalGPIOPin *pin);
  void dump_config();
  void queue_for_send(const Message &&frame) { LockGuard send_lock(send_mutex_); send_queue_.push(std::move(frame)); }
  bool is_idle() const { return send_queue_.empty() && (transmit_state_ == TransmitState::Idle); }
  void set_uart(uart::UARTComponent *uart) { uart_ = uart; }

  /**
   * Transmit the message on the front of the send_queue out on the CEC line
   */
  void transmit_message();

  /**
   * This method is called from within the receiver isr method at the end of a received message.
   * So, the fields that it updates need to be declared 'volatile'.
   * @param n_bytes is this the length of the message in bytes
   * @param n_acks is the sum of byte 'acknowledges' that were received as 'low'. So 0 <= n_acks <= n_bytes
   */
  void got_end_of_message(uint8_t n_bytes, uint8_t n_acks);
  void got_start_of_activity();

protected:

  /**
   * Send the CEC protocol frane start bit.
   * While doing so, check if another initiator tries to do the same, and if so, abort.
   * @return true: start bit was successful, false: bus collision is detected and abort
   */
  bool send_start_bit();
  void send_bit(bool bit_value);
  bool send_high_and_test();
  void transmit_message_on_gpio(const Message &frame);
  void transmit_message_on_uart(const Message &frame);
  void transmit_byte_on_uart(uint8_t byte, bool is_header, bool is_eom);
  bool transmit_my_address(const uint8_t address);  // send the only first 4 bits with my address and check for bus collision

  std::queue<Message> send_queue_;
  uint8_t transmit_attempts_{0};
  volatile bool receiver_is_busy_{false};
  volatile TransmitState transmit_state_{TransmitState::Idle};
  volatile uint8_t n_bytes_received_{0};
  volatile uint8_t n_acks_received_{0};
  volatile uint32_t confirm_received_us_{0};
  uint32_t allow_xmit_message_us_;
  uart::UARTComponent *uart_{nullptr};
  InternalGPIOPin *pin_{nullptr};
  Mutex send_mutex_;
};

class HDMICEC : public Component {
public:
  void set_pin(InternalGPIOPin *pin);
  void set_address(uint8_t address) { address_ = address; }
  uint8_t address() { return address_; }
  void set_physical_address(uint16_t physical_address) { physical_address_ = physical_address; }
  void set_promiscuous_mode(bool promiscuous_mode) { promiscuous_mode_ = promiscuous_mode; }
  void set_monitor_mode(bool monitor_mode) { monitor_mode_ = monitor_mode; }
  void set_osd_name_bytes(const std::vector<uint8_t> &osd_name_bytes) { osd_name_bytes_ = osd_name_bytes; }
  void set_uart(uart::UARTComponent *uart) { xmit_.set_uart(uart); }
  void add_message_trigger(MessageTrigger *trigger) { message_triggers_.push_back(trigger); }

  bool send(uint8_t destination, const std::vector<uint8_t> &data_bytes);
  bool send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes);

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

protected:
  static void gpio_intr_(HDMICEC *self);
  static void reset_state_variables_(HDMICEC *self);
  void try_builtin_handler_(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data);
  void handle_received_message(const Message &frame);


  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin isr_pin_;
  uint8_t address_;
  uint16_t physical_address_;
  bool promiscuous_mode_;
  bool monitor_mode_;
  std::vector<uint8_t> osd_name_bytes_;
  std::vector<MessageTrigger*> message_triggers_;

  CECTransmit xmit_;

  uint32_t last_falling_edge_us_;
  ReceiverState receiver_state_;
  volatile uint8_t recv_bit_counter_;
  volatile uint8_t recv_byte_buffer_;
  Message recv_frame_buffer_;
  std::queue<Message> recv_queue_;
  uint8_t num_acks_;  // numer of 'low' acknowledge bits received in the current message
  bool recv_ack_queued_;
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
  TEMPLATABLE_VALUE(uint8_t, source)
  TEMPLATABLE_VALUE(uint8_t, destination)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) override {
    auto source_address = source_.has_value() ? source_.value(x...) : parent_->address();
    auto destination_address = destination_.value(x...);
    auto data = data_.value(x...);
    parent_->send(source_address, destination_address, data);
  }

protected:
  HDMICEC *parent_;
};

}
}
