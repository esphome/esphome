#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <initializer_list>
#include <span>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome::hdmi_cec {

// Release/acquire store and load. The read-modify-write forms of std::atomic are not available on
// every target (the ESP8266 has none), so a writer computes the new value itself; that is safe here
// because every atomic in this component has exactly one writer.
template<typename T> ESPHOME_ALWAYS_INLINE void atomic_set(std::atomic<T> &atom, T value) {
  atom.store(value, std::memory_order_release);
}
template<typename T> ESPHOME_ALWAYS_INLINE T atomic_get(const std::atomic<T> &atom) {
  return atom.load(std::memory_order_acquire);
}

/// One CEC bus message: a header byte with the initiator and destination addresses, then up to
/// 15 payload bytes of which the first is the opcode. Fixed storage, so the receive interrupt
/// never allocates.
class Frame {
 public:
  static constexpr size_t MAX_LENGTH = 16;  // HDMI-CEC 1.4
  static constexpr size_t MAX_PAYLOAD_LENGTH = MAX_LENGTH - 1;

  Frame() = default;
  /// The payload is at most MAX_PAYLOAD_LENGTH bytes; send() checks that
  Frame(uint8_t initiator_addr, uint8_t destination_addr, std::span<const uint8_t> payload);

  ESPHOME_ALWAYS_INLINE uint8_t initiator_addr() const { return this->data_[0] >> 4; }
  ESPHOME_ALWAYS_INLINE uint8_t destination_addr() const { return this->data_[0] & 0xf; }
  ESPHOME_ALWAYS_INLINE bool is_broadcast() const { return this->destination_addr() == 0xf; }
  ESPHOME_ALWAYS_INLINE size_t size() const { return this->size_; }
  std::span<const uint8_t> bytes() const { return {this->data_.data(), this->size_}; }
  /// The bytes after the header, starting with the opcode
  std::span<const uint8_t> payload() const { return this->bytes().subspan(1); }
  ESPHOME_ALWAYS_INLINE void clear() { this->size_ = 0; }
  /// False when the frame is full already; the bus allows no more
  ESPHOME_ALWAYS_INLINE bool push_back(uint8_t data) {
    if (this->size_ == MAX_LENGTH) {
      return false;
    }
    this->data_[this->size_++] = data;
    return true;
  }

 protected:
  std::array<uint8_t, MAX_LENGTH> data_{};
  uint8_t size_{0};
};

/// A queue of frames between one producer and one consumer, for instance the receive interrupt and
/// loop(). The indices run freely and each is written by one side only, so no lock is needed.
/// back() hands out a slot to fill and push_back() commits it; front() and pop_front() do the same
/// on the reading side.
template<size_t SIZE> class FrameRingBuffer {
  static_assert((SIZE & (SIZE - 1)) == 0, "SIZE must be a power of two");

 public:
  ESPHOME_ALWAYS_INLINE Frame *front() { return this->count_() == 0 ? nullptr : this->slot_(this->front_inx_); }
  ESPHOME_ALWAYS_INLINE void pop_front() { this->advance_(this->front_inx_); }
  ESPHOME_ALWAYS_INLINE Frame *back() { return this->count_() == SIZE ? nullptr : this->slot_(this->back_inx_); }
  ESPHOME_ALWAYS_INLINE void push_back() { this->advance_(this->back_inx_); }
  ESPHOME_ALWAYS_INLINE bool is_empty() const { return this->count_() == 0; }

 protected:
  ESPHOME_ALWAYS_INLINE uint8_t count_() const {
    return static_cast<uint8_t>(atomic_get(this->back_inx_) - atomic_get(this->front_inx_));
  }
  ESPHOME_ALWAYS_INLINE Frame *slot_(const std::atomic<uint8_t> &inx) { return &this->store_[atomic_get(inx) % SIZE]; }
  ESPHOME_ALWAYS_INLINE void advance_(std::atomic<uint8_t> &inx) {
    atomic_set<uint8_t>(inx, static_cast<uint8_t>(atomic_get(inx) + 1));
  }

  std::array<Frame, SIZE> store_{};
  std::atomic<uint8_t> front_inx_{0};
  std::atomic<uint8_t> back_inx_{0};
};

/// Sends frames on the bus. A frame is queued and sent from loop(), with retries spread over later
/// passes, because one transmission keeps the CPU for up to 400 ms and may have to be repeated.
class CECTransmit {
 public:
  static constexpr uint32_t TOTAL_BIT_US = 2400;
  static constexpr uint32_t SIGNAL_FREE_TIME_NEW_INITIATOR = TOTAL_BIT_US * 5;

  void setup(ISRInternalGPIOPin isr_pin);
  ISRInternalGPIOPin isr_pin() const { return this->isr_pin_; }
  bool queue_for_send(uint8_t source, uint8_t destination, std::span<const uint8_t> payload);
  bool is_idle() const { return !this->transmitting_ && this->send_queue_.is_empty(); }
  /// Finish the previous attempt and send the frame at the front of the queue; called from loop()
  void transmit_message();

  // Called from the receive interrupt
  ESPHOME_ALWAYS_INLINE void got_edge(uint32_t now_us) { atomic_set(this->last_edge_us_, now_us); }
  void got_byte_eom_ack(bool eom, bool ack);
  /// Pull the line low for the acknowledge bit of a frame addressed to us
  void send_ack();

 protected:
  ESPHOME_ALWAYS_INLINE void set_pin_input_high_() { this->isr_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }
  ESPHOME_ALWAYS_INLINE void set_pin_output_low_() {
    this->isr_pin_.pin_mode(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
    this->isr_pin_.digital_write(false);
  }
  /// Drive the frame onto the line; false when another initiator sent at the same time
  bool transmit_frame_(const Frame *frame);
  /// Keep the frame for another attempt, or drop it once the attempts are used up
  void finish_attempt_(bool sent_ok);
  bool send_start_bit_();
  void send_bit_(bool bit_value);
  bool send_high_and_test_();

  static constexpr size_t SEND_QUEUE_SIZE = 4;
  FrameRingBuffer<SEND_QUEUE_SIZE> send_queue_;
  ISRInternalGPIOPin isr_pin_;
  std::atomic<uint32_t> last_edge_us_{0};  // the bus is busy until this is some time ago
  uint32_t last_attempt_us_{0};            // a refused start bit drives no edge, so it is timed here
  uint32_t required_idle_period_{SIGNAL_FREE_TIME_NEW_INITIATOR};
  uint8_t transmit_attempts_{0};
  // The interrupt tallies our own frame while transmitting_ is set and no end of message has
  // passed yet; loop() reads the tally once eom_received_ says the frame is over
  uint8_t n_bytes_received_{0};
  uint8_t n_acks_received_{0};
  bool transmitting_{false};
  std::atomic<bool> eom_received_{false};  // set for every frame, ours or not
};

/// Decodes the bus from pin edges in an interrupt and queues complete frames for loop()
class CECReceive {
 public:
  CECReceive(CECTransmit &xmit, uint8_t address) : xmit_(xmit), address_(address) {}
  void setup(InternalGPIOPin *pin, Component *parent);
  uint8_t address() const { return this->address_; }
  void set_promiscuous_mode(bool promiscuous) { this->promiscuous_mode_ = promiscuous; }
  bool get_promiscuous_mode() const { return this->promiscuous_mode_; }
  void set_monitor_mode(bool monitor_mode) { this->monitor_mode_ = monitor_mode; }
  bool get_monitor_mode() const { return this->monitor_mode_; }
  const Frame *next_frame() { return this->frames_queue_.front(); }
  void pop_frame() { this->frames_queue_.pop_front(); }
  /// Frames lost because the queue was full, since the last call
  uint8_t take_dropped_frames();

 protected:
  enum class ReceiverState : uint8_t {
    RECEIVER_STATE_IDLE,
    RECEIVER_STATE_RECEIVING_BYTE,
    RECEIVER_STATE_WAITING_FOR_EOM,
    RECEIVER_STATE_WAITING_FOR_ACK,
    RECEIVER_STATE_WAITING_FOR_EOM_ACK,
  };

  static void gpio_isr_s(CECReceive *self);
  void gpio_isr_();
  void reset_state_variables_();

  static constexpr size_t MAX_FRAMES_QUEUED = 4;
  FrameRingBuffer<MAX_FRAMES_QUEUED> frames_queue_;
  CECTransmit &xmit_;  // acknowledges and bus timing go through the transmitter
  Component *parent_{nullptr};
  ISRInternalGPIOPin isr_pin_;
  Frame *recv_frame_buffer_{nullptr};
  uint32_t last_falling_edge_us_{0};
  std::atomic<uint8_t> dropped_frames_{0};  // written by the interrupt only
  uint8_t reported_dropped_frames_{0};
  uint8_t address_;
  uint8_t recv_bit_counter_{0};
  uint8_t recv_byte_buffer_{0};
  ReceiverState receiver_state_{ReceiverState::RECEIVER_STATE_IDLE};
  bool promiscuous_mode_{false};
  bool monitor_mode_{false};
  bool recv_ack_queued_{false};
  bool prev_pin_level_{true};
};

/// For the generated on_message filters: does the payload start with these bytes?
inline bool starts_with(std::span<const uint8_t> data, std::initializer_list<uint8_t> prefix) {
  return data.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin());
}

class HDMICEC : public Component {
 public:
  HDMICEC(InternalGPIOPin *pin, uint8_t address) : pin_(pin), recv_(xmit_, address) {}
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_promiscuous_mode(bool promiscuous) { this->recv_.set_promiscuous_mode(promiscuous); }
  void set_monitor_mode(bool monitor_mode) { this->recv_.set_monitor_mode(monitor_mode); }
#ifdef HDMI_CEC_MESSAGE_CALLBACK_COUNT
  /// Called with the initiator, the destination and the payload (opcode first) of every received
  /// frame; the payload is only valid during the call
  template<typename F> void add_on_message_callback(F &&callback) {
    this->message_callbacks_.add(std::forward<F>(callback));
  }
#endif

  /// Queue a frame for transmission; false when the queue is full or in monitor mode
  bool send(uint8_t source, uint8_t destination, std::span<const uint8_t> payload);
  bool send(uint8_t destination, std::span<const uint8_t> payload) {
    return this->send(this->recv_.address(), destination, payload);
  }
  /// For lambdas: id(cec).send(0x0, {0x44, 0x41});
  bool send(uint8_t destination, std::initializer_list<uint8_t> payload) {
    return this->send(destination, std::span<const uint8_t>(payload.begin(), payload.size()));
  }

 protected:
  InternalGPIOPin *pin_;
#ifdef HDMI_CEC_MESSAGE_CALLBACK_COUNT
  StaticCallbackManager<HDMI_CEC_MESSAGE_CALLBACK_COUNT, void(uint8_t, uint8_t, std::span<const uint8_t>)>
      message_callbacks_;
#endif
  CECTransmit xmit_;
  CECReceive recv_;
};

}  // namespace esphome::hdmi_cec
