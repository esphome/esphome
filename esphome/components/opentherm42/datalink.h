#pragma once

#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include "driver/gptimer.h"
#endif

namespace esphome::opentherm42 {

// §4.2: 32-bit frame, MSB first: P(1) MSG-TYPE(3) SPARE(4) | DATA-ID(8) | DATA-VALUE(16).
struct Frame {
  uint8_t type{0};
  uint8_t id{0};
  uint8_t value_hb{0};
  uint8_t value_lb{0};

  uint16_t value_u16() const { return (static_cast<uint16_t>(this->value_hb) << 8) | this->value_lb; }
  void set_value_u16(uint16_t value) {
    this->value_hb = static_cast<uint8_t>(value >> 8);
    this->value_lb = static_cast<uint8_t>(value & 0xFF);
  }
  int16_t value_s16() const { return static_cast<int16_t>(this->value_u16()); }
  void set_value_s16(int16_t value) { this->set_value_u16(static_cast<uint16_t>(value)); }
  // f8.8: signed fixed point, 1 sign bit + 7 integer bits + 8 fractional bits (§5.1).
  float value_f88() const { return static_cast<float>(this->value_s16()) / 256.0f; }
  void set_value_f88(float value) { this->set_value_s16(static_cast<int16_t>(value * 256.0f)); }
};

// §4.2.2: the 3-bit MSG-TYPE field. Master-to-boiler uses READ_DATA/WRITE_DATA/INVALID_DATA (011 is
// reserved); boiler-to-master uses READ_ACK/WRITE_ACK/DATA_INVALID/UNKNOWN_DATA_ID.
enum class MessageType : uint8_t {
  READ_DATA = 0b000,
  WRITE_DATA = 0b001,
  INVALID_DATA = 0b010,
  // 0b011 reserved
  READ_ACK = 0b100,
  WRITE_ACK = 0b101,
  DATA_INVALID = 0b110,
  UNKNOWN_DATA_ID = 0b111,
};

// Every way a frame can fail to be usable, at the bit level (§3.3.3, §4.2.1) or the conversation level
// (§4.3.1, §4.5). One enum so every caller reports failures the same way instead of inventing ad hoc
// error strings -- see error_to_string() below.
enum class DataLinkError : uint8_t {
  NONE = 0,
  // §3.3.1/§3.3.3: no mid-bit transition where Manchester encoding requires one.
  MANCHESTER_NO_TRANSITION,
  // §3.3.2: the line stayed at one level far longer than the 900-1150 µs mid-bit transition window --
  // either the line is stuck, or the sender stopped mid-frame.
  MANCHESTER_TIMEOUT,
  // §4.2: the frame didn't end with a stop bit ('1') where expected.
  INVALID_STOP_BIT,
  // §4.2.1: the total number of '1' bits across the 32-bit frame is odd.
  PARITY_ERROR,
  // §4.3.1: no start bit seen from the boiler within the answering-time window (20-400 ms after the
  // master's transmission ended).
  RESPONSE_TIMEOUT,
  // Hardware timer could not be configured/armed/read -- see TimerError for which operation failed.
  TIMER_ERROR,
};

const char *data_link_error_to_string(DataLinkError error);

// Which specific timer operation failed when DataLinkError::TIMER_ERROR is reported. ESP32-only (the
// ESP8266 timer API doesn't return per-operation error codes).
enum class TimerError : uint8_t {
  NONE = 0,
  CREATE,
  REGISTER_CALLBACK,
  ENABLE,
  SET_ALARM,
  START,
  STOP,
};

const char *timer_error_to_string(TimerError error);

enum class DataLinkState : uint8_t {
  IDLE,
  LISTENING,  // waiting for the boiler's response start bit
  RECEIVING,  // decoding an in-progress incoming frame
  RECEIVED,   // a full, valid frame is available via OpenThermDataLink::get_frame()
  SENDING,    // transmitting an outgoing frame
  SENT,       // the outgoing frame was fully transmitted
  ERROR,      // see OpenThermDataLink::get_error()
};

// Implements the OpenTherm Protocol Specification v4.2, chapter 4 (DataLink Layer): Manchester bit
// encoding/decoding, 32-bit frame assembly, and detection of every error case chapter 4 and §3.3.3
// define. Conversation-level scheduling (§4.3: master-initiated request/response pairs, timing between
// conversations) is the caller's responsibility -- this class only sends and receives single frames.
//
// Bit sampling runs from a hardware timer callback (IRAM-resident on ESP32/ESP8266) at 5x the nominal
// bit rate (5 kHz, i.e. every 200 µs) while receiving, and at 2x the bit rate (2 kHz) while transmitting
// -- fast enough to resolve the mid-bit transition against the spec's 100-150 µs acceptance window
// (§3.3.2) without needing GPIO edge interrupts, whose latency is less predictable across platforms.
class OpenThermDataLink {
 public:
  OpenThermDataLink(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  // Configures the pins and (on ESP32) the hardware timer. Returns false if the timer could not be set
  // up -- check get_timer_error() for why.
  bool initialize();

  // Starts listening for a response frame from the boiler. response_timeout_ms bounds how long to wait
  // for the response's start bit (§4.3.1: 20-400 ms is the legal range for a compliant boiler) before
  // reporting DataLinkError::RESPONSE_TIMEOUT.
  void listen(uint32_t response_timeout_ms);

  // Starts transmitting a frame. The parity bit (§4.2.1) is computed and set automatically.
  void send(const Frame &frame);

  // Disarms the timer, resets to IDLE, and drives the output pin back to its idle level. Call this once
  // the caller is done inspecting a terminal state (get_frame()/get_error()) to prepare for the next
  // listen()/send(). Safe to call from any state.
  void stop();

  DataLinkState get_state() const { return this->state_; }
  bool is_idle() const { return this->state_ == DataLinkState::IDLE; }
  bool has_frame() const { return this->state_ == DataLinkState::RECEIVED; }
  bool is_sent() const { return this->state_ == DataLinkState::SENT; }
  bool has_error() const { return this->state_ == DataLinkState::ERROR; }

  // Only valid when has_frame() is true.
  Frame get_frame() const { return this->frame_; }
  // Only valid when has_error() is true.
  DataLinkError get_error() const { return this->error_; }
  // Only valid when has_error() is true and get_error() == DataLinkError::TIMER_ERROR.
  TimerError get_timer_error() const { return this->timer_error_; }

#ifdef USE_ESP32
  static bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
#else
  static void timer_isr();
#endif

 protected:
  // Ported from a proven, hardware-verified Manchester decoder (esphome/components/opentherm/opentherm.cpp
  // as of this repository's a811aa840c) -- deliberately kept close to that structure and variable roles
  // rather than rewritten from the spec text, since a subtle bit-timing mistake here can't be caught by
  // compilation or config validation, only by testing against a real boiler.
  // IRAM_ATTR belongs only on the .cpp definitions (see datalink.cpp) -- putting it on both the
  // declaration and the definition gives each a different auto-numbered .iram1.N section, which the
  // compiler then warns about as conflicting attributes.
  void on_timer_tick_();
  void record_bit_(uint8_t value);
  DataLinkError check_stop_bit_(uint8_t value);
  void write_bit_(uint8_t high, uint8_t clock);
  static bool check_parity_(uint32_t frame_bits);
  // Sets state_ to ERROR and records which error -- callers must follow this with stop_timer_(), not
  // stop() (which would immediately overwrite state_ back to IDLE).
  void set_error_(DataLinkError error);

  // Disarms the hardware timer only -- does NOT touch state_/error_ or the output pin. Called from inside
  // the timer ISR once a conversation reaches a terminal state (RECEIVED/SENT/ERROR), so that terminal
  // state survives for the caller to inspect via get_state()/get_error() instead of being clobbered by a
  // reset back to IDLE. Contrast with the public stop(), which is what actually resets to IDLE.
  void stop_timer_();

  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;

#ifdef USE_ESP32
  gptimer_handle_t timer_handle_{nullptr};
#endif

  DataLinkState state_{DataLinkState::IDLE};
  DataLinkError error_{DataLinkError::NONE};
  TimerError timer_error_{TimerError::NONE};

  Frame frame_;

  // §3.3.1 Manchester decode, sampled every 200 µs (5x the nominal 1 kHz bit rate).
  //
  // `capture_` is a shift register of the most recent raw pin samples: bit 0 (`capture_ & 1`) is the
  // previous sample, compared against the newly-read level to detect a transition; its overall magnitude
  // also doubles as an elapsed-tick counter since the last transition (each non-transition tick shifts
  // another 0 or 1 in without resetting it). `clock_` alternates which of the two transitions per bit
  // period we're expecting next: 1 for the mandatory mid-bit (data) edge, 0 for the optional bit-boundary
  // edge. `data_` accumulates the decoded data+parity bits (MSB-first, start bit not stored); `bit_pos_`
  // counts how many of those 33 bits (32 data/parity + stop) have been captured so far.
  uint32_t capture_{0};
  uint8_t clock_{1};
  uint32_t data_{0};
  uint8_t bit_pos_{0};
  // While LISTENING for the response start bit: counts down 200 µs ticks, -1 once disabled.
  int32_t listen_ticks_remaining_{-1};

  // §3.3.1 Manchester encode, clocked every 500 µs (2x the nominal bit rate: two half-bit writes per
  // bit). `tx_bit_pos_` counts down from 33 (start bit) through 1 (last data/parity bit) to 0 (stop bit).
  uint32_t tx_data_{0};
  int8_t tx_bit_pos_{0};
  uint8_t tx_clock_{1};
};

}  // namespace esphome::opentherm42
