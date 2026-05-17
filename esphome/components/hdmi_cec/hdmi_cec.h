#pragma once

#include <array>
#include <vector>
#include <atomic>
#include <initializer_list>

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

// For atomics, the '++' operator is explicitly avoided here, because that requires an atomic 'read-modify-write'
// which is not supported by all targets. (In particular not by the esp8266)
// The fall-back here to a non-atomic read-modify-write is OK for this application, as no other thread (or isr)
// will modify the same variable.
// The 'atomic' semantics is required here for its memory-barrier.
#define ATOMIC_SET(atom, value) (atom).store((value), std::memory_order_release)
#define ATOMIC_INCR(atom) ATOMIC_SET(atom, (atom) + 1)
#define ATOMIC_GET(atom) (atom).load(std::memory_order_acquire)

/**
 * A Frame stores an HDMI-CEC bus message as sequence of bytes with variable length.
 * Such bus message is either received or to be transmitted.
 * The naming 'frame' corresponds to the nomenclature in the HDMI-CEC standard.
 * A frame contains one 'header' byte and a variable number of 'payload' bytes.
 * The HDMI CEC standard 1.4 specifies a maximum length of 16 for any frame.
 * Filling a frame with 'push_back' in an ISR shall not cause dynamic memory allocation.
 */
class Frame {
 public:
  Frame() = default;
  Frame(uint8_t initiator_addr, uint8_t target_addr, const uint8_t *payload, unsigned int payload_size);
  Frame(uint8_t initiator_addr, uint8_t target_addr, const std::initializer_list<uint8_t> &payload)
      : Frame(initiator_addr, target_addr, payload.begin(), payload.size()) {}

  void set_header(uint8_t initiator_addr, uint8_t target_addr);
  uint8_t initiator_addr() const { return (data_[0] >> 4) & 0xf; }
  uint8_t destination_addr() const { return data_[0] & 0xf; }
  uint8_t opcode() const { return (this->size() >= 2) ? data_[1] : 0; }
  bool is_broadcast() const { return this->destination_addr() == 0xf; }
  uint8_t size() const { return size_; }
  uint8_t at(unsigned int i) const { return (i < size_) ? data_[i] : 0; }
  uint8_t operator[](unsigned int i) const { return at(i); }
  void clear() { size_ = 0; }
  void push_back(uint8_t data) {
    if (size_ < data_.size()) {
      data_[size_++] = data;
    }
  }
  std::string to_string() const;  // NOLINT
  constexpr static int MAX_LENGTH = 16;

 protected:
  std::array<uint8_t, MAX_LENGTH> data_;
  uint8_t size_{0};
};

/**
 * The FrameRingBuffer is a container for Frames to queue data in a consumer-producer
 * application. The use of std::Atomics allows safe multi-thread operation when used with
 * a single producer and single consumer thread, where each Atomic index is updated
 * by one thread only.
 * After initialization, it operates without dynamic memory allocation.
 * This allows the gpio isr to safely and efficiently pick-up and pass Frames.
 * Due to its fixed memory size, it might return NULL pointers in case the buffer is full or empty.
 */
template<unsigned int SIZE> class FrameRingBuffer {
 public:
  FrameRingBuffer() : front_inx_{0}, back_inx_{0}, store_{} {
    for (auto &t : store_) {
      t = new Frame;
    }
  }
  ~FrameRingBuffer() {
    for (auto &t : store_) {
      delete t;
    }
  }
  // 'front' is used to access a Frame, and use its content until it is no longer needed
  Frame *front() const { return is_empty() ? nullptr : store_[front_inx_]; }
  // 'pop_front' recycles the storage area of the last 'front()' call.
  // This invalidates further use of that ptr by the caller
  void pop_front() { cyclic_incr_(front_inx_); }
  // 'back' is used to fetch a free Frame, fill with data, and queue for later pick-up
  // Note that its returned Frame likely contains 'dirty' data.
  Frame *back() const { return is_full() ? nullptr : store_[back_inx_]; }
  // 'push_back' commits the frame that was earlier obtained by 'back()', presumably it got filled with content
  // This invalidates further use of this frame by the caller.
  void push_back() { cyclic_incr_(back_inx_); }
  bool is_empty() const { return count_() == 0; }
  bool is_full() const { return count_() == SIZE; }
  void reset() {
    front_inx_ = 0;
    back_inx_ = 0;
  }

 protected:
  using Index = std::atomic<unsigned int>;
  // this simple increment scheme is sufficiently 'atomic' if the front and back are each used by
  // one thread only. (So, at most one reader thread and one writer thread in the application.)
  int count_() const {
    int n = (int) (ATOMIC_GET(back_inx_) - ATOMIC_GET(front_inx_));
    if (n < 0)
      n += SIZE + 1;
    return n;
  }
  void cyclic_incr_(Index &inx) { ATOMIC_SET(inx, (inx == SIZE) ? 0 : (inx + 1)); }
  Index front_inx_;  // ranging 0 .. SIZE
  Index back_inx_;   // ranging 0 .. SIZE
  // if front_inx_ == back_inx_ the store is considered empty, so it can hold at most SIZE elements
  std::array<Frame *, SIZE + 1> store_;
};

/**
 * class CECTransmit accepts bus messages ("frames") for transmission.
 * These messages, taken from "queue_for_send" are buffered, because a 'send' on the bus
 * might take a really long time, which should not be directly consumed in its calling thread:
 * With the local buffering, 'send' retries are now handled in subsequent time slices.
 * The 'send' queue has a limited depth, fairly shallow, because in practice an application
 * is expected not to queue so many 'send's before requesting (waiting for) an
 * inbound reply message.
 */
class CECTransmit {
  enum class TransmitState : uint8_t {
    IDLE,
    BUSY,
    EOM_CONFIRMED,
  };

 public:
  void setup(InternalGPIOPin *pin);
  void dump_config();
  bool queue_for_send(uint8_t source, uint8_t destination, const uint8_t *payload_bytes, unsigned int payload_size);
  bool is_idle() const { return send_queue_.is_empty() && (transmit_state_ == TransmitState::IDLE); }
  void set_uart(uart::UARTComponent *uart) { uart_ = uart; }
  bool has_uart() const { return uart_ != nullptr; }
  // Note: 'ESPHOME_ALWAYS_INLINE' allows the following methods to be also used in 'IRAM_ATTR' methods.
  ESPHOME_ALWAYS_INLINE void set_pin_input_high() { pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }
  ESPHOME_ALWAYS_INLINE void set_pin_output_low() {
    pin_->pin_mode(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
    pin_->digital_write(false);
  }

  /**
   * Transmit the message on the front of the send_queue out on the CEC line
   */
  void transmit_message();

  /**
   * This method is called from within the receiver isr method after receiving each byte.
   * So, the fields that it updates need to be 'atomic'.
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
   * Send the CEC protocol frame start bit.
   * While doing so, check if another initiator tries to do the same, and if so, abort.
   * @return true: start bit was successful, false: bus collision is detected and abort
   */
  bool send_start_bit_();
  void send_bit_(bool bit_value);
  bool send_high_and_test_();
  void transmit_message_on_gpio_(const Frame *frame);
  void transmit_message_on_uart_(const Frame *frame);
  void convert_byte_to_uart_(std::vector<uint8_t> &uart_data, uint8_t byte, bool is_header, bool is_eom);
  bool transmit_my_address_(uint8_t initiator_addr);  // send 4 bits with my address and check for bus collision

  FrameRingBuffer<5> send_queue_;  // queue at most a handful of messages to send
  uint8_t transmit_attempts_{0};
  uint8_t n_bytes_received_{0};
  uint8_t n_acks_received_{0};
  TransmitState transmit_state_{TransmitState::IDLE};
  std::atomic<bool> eom_received_{false};
  std::atomic<bool> receiver_is_busy_{false};
  uint32_t confirm_received_us_{0};
  uint32_t required_idle_period_{0};
  uart::UARTComponent *uart_{nullptr};
  InternalGPIOPin *pin_{nullptr};
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
  FrameRingBuffer<MAX_FRAMES_QUEUED> frames_queue_;

 protected:
  static void gpio_isr_s(CECReceive *self);
  void gpio_isr_();
  void reset_state_variables_();

  CECTransmit &xmit_;  // the bus 'Receiver' needs to sync with 'sent' messages
  ISRInternalGPIOPin isr_pin_;
  bool promiscuous_mode_{false};
  bool monitor_mode_{false};
  bool recv_ack_queued_{false};
  bool prev_pin_level_{true};
  uint8_t recv_bit_counter_{0};
  uint8_t recv_byte_buffer_{0};
  uint8_t num_acks_{0};  // numer of 'low' acknowledge bits received in the current message
  uint8_t address_{0};
  uint32_t last_falling_edge_us_{0};
  ReceiverState receiver_state_{ReceiverState::IDLE};
  Frame *recv_frame_buffer_{nullptr};
};

class HDMICEC : public Component {
 public:
  void set_pin(InternalGPIOPin *pin);
  void set_address(uint8_t address) { address_ = address & 0xf; }
  uint8_t address() const { return address_; }
  void set_physical_address(uint16_t physical_address) { physical_address_ = physical_address; }
  uint16_t get_physical_address() const { return physical_address_; }
  void set_promiscuous_mode(bool promiscuous) { recv_.set_promiscuous_mode(promiscuous); }
  void set_monitor_mode(bool monitor_mode) { recv_.set_monitor_mode(monitor_mode); }
  void set_osd_name_bytes(const std::vector<uint8_t> &osd_name_bytes) { osd_name_bytes_ = osd_name_bytes; }
  void set_uart(uart::UARTComponent *uart) { xmit_.set_uart(uart); }
  void add_message_trigger(MessageTrigger *trigger) { message_triggers_.push_back(trigger); }
  uint8_t get_device_type() const;

  // Generic 'send' of a bus message, it pushes its message into a send buffer for later transmit.
  bool send(uint8_t source, uint8_t destination, const uint8_t *payload_bytes, unsigned int payload_size);

  // More compact 'send' wrappers for easier use:
  // Preferred for most use cases, with a compile-time-fixed argument list:
  bool send(uint8_t destination, const std::initializer_list<uint8_t> &payload) {
    return send(address_, destination, payload.begin(), payload.size());
  }
  // For use when the payload is stored by the caller in an std::vector or std::array:
  bool send(uint8_t destination, const uint8_t *payload_bytes, unsigned int payload_size) {
    return send(address_, destination, payload_bytes, payload_size);
  }
  // And finally, provided for backwards compatibility, now discouraged:
  // Better avoid use of a 'source' != my address_. Better avoid the 'vector' for its heap allocation
  bool send(uint8_t source, uint8_t destination, const std::vector<uint8_t> &payload) {
    return send(source, destination, payload.data(), payload.size());
  }

  // Component overrides
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  void try_builtin_handler_(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data);
  void handle_received_message_(const Frame *frame);

  HighFrequencyLoopRequester fast_loop_;
  InternalGPIOPin *pin_{nullptr};
  uint8_t address_;  // logical address, relates to device type
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

  void play(const Ts &...x) override {
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
