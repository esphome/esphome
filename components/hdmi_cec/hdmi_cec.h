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

std::string bytes_to_string(std::vector<uint8_t> bytes);

enum class ReceiverState : uint8_t {
  Idle = 0,
  ReceivingByte = 2,
  WaitingForEOM = 3,
  WaitingForAck = 4,
  WaitingForEOMAck = 5,
};

enum class TransmitState : uint8_t {
  Idle       = 0,
  Busy       = 1,
  EomAckGood = 2,
  EomAckFail = 3,
};

class MessageTrigger;
using Message = std::vector<uint8_t>;

class HDMICEC : public Component {
public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_address(uint8_t address) { address_ = address; }
  uint8_t address() { return address_; }
  void set_physical_address(uint16_t physical_address) { physical_address_ = physical_address; }
  void set_promiscuous_mode(bool promiscuous_mode) { promiscuous_mode_ = promiscuous_mode; }
  void set_monitor_mode(bool monitor_mode) { monitor_mode_ = monitor_mode; }
  void set_osd_name_bytes(const std::vector<uint8_t> &osd_name_bytes) { osd_name_bytes_ = osd_name_bytes; }
  void set_uart(uart::UARTComponent *uart) { uart_ = uart; }
  void add_message_trigger(MessageTrigger *trigger) { message_triggers_.push_back(trigger); }

  bool send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data_bytes);

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

protected:
  static void gpio_intr_(HDMICEC *self);
  static void reset_state_variables_(HDMICEC *self);
  void try_builtin_handler_(uint8_t source, uint8_t destination, const Message &data);
  bool send_frame_(const Message &frame, bool is_broadcast);
  bool send_start_bit_();
  void send_bit_(bool bit_value);
  bool send_and_read_ack_(bool is_broadcast);
  void switch_to_listen_mode_();
  void switch_to_send_mode_();
  void handle_received_message(const Message &frame);

  /** Transmit one messge out on the CEC line
   * @param frame Message to send
   * @return true: send is finished, message can be removed from queue; false: transmit needs another attempt
  */
  void transmit_message();
  void transmit_message_on_gpio(const Message &frame);
  void transmit_message_on_uart(const Message &frame);
  void transmit_byte_on_uart(uint8_t byte, bool is_header, bool is_eom);
  bool transmit_my_address(const uint8_t &address);  // send the only first 4 bits with my address and check for bus collision
  void transmit_receives_ack(bool isEom, bool ack);

  std::queue<Message> send_queue_;
  TransmitState transmit_state_{TransmitState::Idle};
  uint8_t transmit_attempts_{0};
  volatile bool transmit_acks_ok_{true};
  volatile bool transmit_bus_collision_{false};
  uart::UARTComponent *uart_{nullptr};

  InternalGPIOPin *pin_;
  ISRInternalGPIOPin isr_pin_;
  uint8_t address_;
  uint16_t physical_address_;
  bool promiscuous_mode_;
  bool monitor_mode_;
  std::vector<uint8_t> osd_name_bytes_;
  std::vector<MessageTrigger*> message_triggers_;

  uint32_t last_falling_edge_us_;
  uint32_t allow_xmit_message_us_;
  ReceiverState receiver_state_;
  volatile uint8_t recv_bit_counter_;
  volatile uint8_t recv_byte_buffer_;
  Message recv_frame_buffer_;
  std::queue<Message> recv_queue_;
  bool recv_ack_queued_;
  Mutex send_mutex_;
};

class MessageTrigger : public Trigger<uint8_t, uint8_t, Message> {
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
