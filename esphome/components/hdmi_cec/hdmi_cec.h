#pragma once

#include <array>
#include <vector>
#include <queue>
#include <atomic>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace uart {
class UARTComponent;
}
namespace hdmi_cec {

class MessageTrigger;

class Message : public std::vector<uint8_t> {
 public:
  Message() = default;
  Message(uint8_t initiator_addr, uint8_t target_addr, const std::vector<uint8_t> &payload);
  uint8_t initiator_addr() const { return (this->at(0) >> 4) & 0xf; }
  uint8_t destination_addr() const { return this->at(0) & 0xf; }
  uint8_t opcode() const { return (this->size() >= 2) ? this->at(1) : 0; }
  bool is_broadcast() const { return this->destination_addr() == 0xf; }
  std::string to_string(uint8_t my_address) const;
  constexpr static int MAX_LENGTH = 16;  // from HDMI CEC standard 1.4
};

/**
 * The MessageRingBuffer is a container for Frames to queue data in a consumer-producer
 * application. The use of std::Atomics allows safe multi-thread operation when used with
 * a single producer and single consumer thread, where each Atomic index is updated
 * by one thread only.
 * After initialization, it operates without dynamic memmory allocation.
 * This allows the gpio isr to safely and efficiently pick-up and pass Frames.
 * Due to its fixed memory size, it might return NULL pointers in case the buffer is full or empty.
 */
template<unsigned int SIZE> class MessageRingBuffer {
 public:
  MessageRingBuffer() : front_inx_{0}, back_inx_{0}, store_{} {
    for (auto &t : store_) {
      t = new Message;
      t->reserve(Message::MAX_LENGTH);
    }
  }
  ~MessageRingBuffer() {
    for (auto &t : store_) {
      delete t;
    }
  }
  // 'front' is used to access data, use that, and recycle its memory space for later use.
  Message *front() const { return is_empty() ? nullptr : store_[front_inx_]; }
  void push_front() { cyclic_incr(front_inx_); }
  // 'back' is used to fetch a free Frame, fill with data, and queue for later pick-up
  Message *back() const { return is_full() ? nullptr : (store_[back_inx_]->clear(), store_[back_inx_]); }
  void push_back() { cyclic_incr(back_inx_); }
  bool is_empty() const { return count() == 0; }
  bool is_full() const { return count() == SIZE; }  // using safe wrap-around of unsignd int
  void reset() {
    front_inx_ = 0;
    back_inx_ = 0;
  }

 protected:
  using Index = std::atomic<unsigned int>;
  // this simple increment scheme is sufficiently 'atomic' if the front and back are each used by
  // one thread only. (So, at most one reader thread and one writer thread in the application.)
  int count() const {
    int n = (int) (back_inx_ - front_inx_);
    if (n < 0)
      n += SIZE + 1;
    return n;
  }
  void cyclic_incr(Index &inx) { inx = (inx == SIZE) ? 0 : (inx + 1); }
  Index front_inx_;  // ranging 0 .. SIZE
  Index back_inx_;   // ranging 0 .. SIZE
  // if front_inx_ == back_inx_ the store is considered empty, so it can hold at most SIZE elements
  std::array<Message *, SIZE + 1> store_;
};

class CECTransmit {
  enum class TransmitState : uint8_t {
    IDLE,
    BUSY,
    EOM_CONFIRMED,
  };

 public:
  void setup(InternalGPIOPin *pin);
  void dump_config();
  void queue_for_send(const Message &&frame);
  bool is_idle() const { return send_queue_.empty() && (transmit_state_ == TransmitState::IDLE); }
  void set_uart(uart::UARTComponent *uart) { uart_ = uart; }
  bool has_uart() const { return uart_ != nullptr; }
  void set_pin_input_high();
  void set_pin_output_low();

  /**
   * Transmit the message on the front of the send_queue out on the CEC line
   */
  void transmit_message();

  /**
   * This method is called from within the receiver isr method after receiving each byte.
   * So, the fields that it updates need to be declared 'volatile'.
   * @param eom the value of the received 'eom' bit
   * @param ack the value of the received 'ack' bit
   */
  void got_byte_eom_ack(bool eom, bool ack);

  /**
   * This method is called from within the receiver isr method,
   * when receiving a start-bit, to notify that the CEC bus line is busy
   */
  void got_start_of_activity();

  /**
   * The transmitter immediatly sends an acknowledge bit on behalf of
   * the receiver, during the receipt of a message.
   * This is either done by activating the UART, if configured and available,
   * otherwise by direct GPIO pin manipulation.
   */
  void send_ack();

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
  void convert_byte_to_uart(std::vector<uint8_t> &uart_data, uint8_t byte, bool is_header, bool is_eom);
  bool transmit_my_address(const uint8_t address);  // send 4 bits with my address and check for bus collision

  std::queue<Message> send_queue_;
  uint8_t transmit_attempts_{0};
  volatile bool receiver_is_busy_{false};
  volatile TransmitState transmit_state_{TransmitState::IDLE};
  volatile uint8_t n_bytes_received_{0};
  volatile uint8_t n_acks_received_{0};
  volatile uint32_t confirm_received_us_{0};
  uint32_t allow_xmit_message_us_;
  uart::UARTComponent *uart_{nullptr};
  InternalGPIOPin *pin_{nullptr};
  Mutex send_mutex_;
};

class CECReceive {
  enum class ReceiverState : uint8_t { IDLE, RECEIVING_BYTE, WAITING_FOR_EOM, WAITING_FOR_ACK, WAITING_FOR_EOM_ACK };

 public:
  constexpr static int MAX_FRAMES_QUEUED = 4;
  CECReceive(CECTransmit &xmit) : xmit_(xmit) {}
  void setup(InternalGPIOPin *pin, uint8_t address);
  void dump_config();
  void set_promiscuous_mode(bool promiscuous) { promiscuous_mode_ = promiscuous; }
  void set_monitor_mode(bool monitor_mode) { monitor_mode_ = monitor_mode; }
  bool get_monitor_mode() const { return monitor_mode_; }
  MessageRingBuffer<MAX_FRAMES_QUEUED> frames_queue_;

 protected:
  static void gpio_isr_s(CECReceive *self);
  void gpio_isr();
  void reset_state_variables();

  CECTransmit &xmit_;
  ISRInternalGPIOPin isr_pin_;
  bool promiscuous_mode_{false};
  bool monitor_mode_{false};
  bool recv_ack_queued_{false};
  bool prev_pin_level_{true};
  volatile uint8_t recv_bit_counter_{0};
  volatile uint8_t recv_byte_buffer_{0};
  uint8_t num_acks_{0};  // numer of 'low' acknowledge bits received in the current message
  uint8_t address_{0};
  uint32_t last_falling_edge_us_{0};
  ReceiverState receiver_state_{ReceiverState::IDLE};
  Message *recv_frame_buffer_{nullptr};
};

class HDMICEC : public Component {
 public:
  void set_pin(InternalGPIOPin *pin);
  void set_address(uint8_t address) { address_ = address & 0xf; }
  uint8_t address() { return address_; }
  void set_physical_address(uint16_t physical_address) { physical_address_ = physical_address; }
  void set_promiscuous_mode(bool promiscuous) { recv_.set_promiscuous_mode(promiscuous); }
  void set_monitor_mode(bool monitor_mode) { recv_.set_monitor_mode(monitor_mode); }
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
  void try_builtin_handler_(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data);
  void handle_received_message(const Message *frame);

  HighFrequencyLoopRequester fast_loop_;
  InternalGPIOPin *pin_{nullptr};
  uint8_t address_;
  uint16_t physical_address_;
  std::vector<uint8_t> osd_name_bytes_;
  std::vector<MessageTrigger *> message_triggers_;

  CECTransmit xmit_;
  CECReceive recv_{xmit_};
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

}  // namespace hdmi_cec
}  // namespace esphome
