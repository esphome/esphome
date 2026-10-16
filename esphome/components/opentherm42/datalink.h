#pragma once

#include "esphome/core/hal.h"

#ifdef OPENTHERM42_DATALINK_RMT
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_timer.h"
#elif defined(USE_ESP32)
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

const char *message_type_to_string(MessageType type);

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
  // ISR backend only: hardware timer could not be configured/armed/read -- see TimerError for which
  // operation failed.
  TIMER_ERROR,
  // RMT backend only: the RMT peripheral driver returned an error -- see RmtError for which
  // operation failed.
  RMT_ERROR,
};

const char *data_link_error_to_string(DataLinkError error);

// Which specific timer operation failed when DataLinkError::TIMER_ERROR is reported. ESP32-only (the
// ESP8266 timer API doesn't return per-operation error codes).
enum class TimerError : uint8_t {
  // Bare words like ENABLE/START/STOP are common vendor-SDK macro names (e.g. LibreTiny's Realtek
  // SDK) -- the preprocessor would replace the enumerator before the compiler ever sees it, so every
  // value is prefixed (see CLAUDE.md's enumerator naming rule).
  TIMER_ERROR_NONE = 0,
  TIMER_ERROR_CREATE,
  TIMER_ERROR_REGISTER_CALLBACK,
  TIMER_ERROR_ENABLE,
  TIMER_ERROR_SET_ALARM,
  TIMER_ERROR_START,
  TIMER_ERROR_STOP,
};

const char *timer_error_to_string(TimerError error);

// Which specific RMT driver call failed when DataLinkError::RMT_ERROR is reported. RMT backend only.
enum class RmtError : uint8_t {
  RMT_ERROR_NONE = 0,
  RMT_ERROR_NEW_TX_CHANNEL,
  RMT_ERROR_NEW_RX_CHANNEL,
  RMT_ERROR_NEW_ENCODER,
  RMT_ERROR_ENABLE,
  RMT_ERROR_TRANSMIT,
  RMT_ERROR_RECEIVE,
};

const char *rmt_error_to_string(RmtError error);

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
// Two backends implement this same interface, chosen at compile time by whether
// OPENTHERM42_DATALINK_RMT is defined (see datalink.cpp vs datalink_rmt.cpp):
//
// - ISR backend (datalink.cpp, always available): bit sampling runs from a hardware timer callback
//   (IRAM-resident on ESP32/ESP8266) at 5x the nominal bit rate (5 kHz, i.e. every 200 µs) while
//   receiving, and at 2x the bit rate (2 kHz) while transmitting -- fast enough to resolve the mid-bit
//   transition against the spec's 100-150 µs acceptance window (§3.3.2). Every sample requires the ISR
//   to run, so a long enough interrupt-mask elsewhere (e.g. another component's InterruptLock) can
//   corrupt an in-flight frame.
// - RMT backend (datalink_rmt.cpp, ESP32 only, on variants with RMT hardware): a 34-bit OT frame is 34
//   RMT symbols, which fits in a single RMT memory block -- the peripheral clocks the whole frame
//   autonomously once started, with no per-sample CPU/ISR involvement, so it isn't vulnerable to the
//   same interrupt-mask collisions as the ISR backend.
class OpenThermDataLink {
 public:
  OpenThermDataLink(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  // Configures the pins and the backend's hardware (timer or RMT channels). Returns false if that setup
  // failed -- check get_error() (and get_timer_error()/get_rmt_error() for the specific operation) for
  // why.
  bool initialize();

  // Starts listening for a response frame from the boiler. response_timeout_ms bounds how long to wait
  // for the response's start bit (§4.3.1: 20-400 ms is the legal range for a compliant boiler) before
  // reporting DataLinkError::RESPONSE_TIMEOUT.
  void listen(uint32_t response_timeout_ms);

  // Starts transmitting a frame. The parity bit (§4.2.1) is computed and set automatically.
  void send(const Frame &frame);

  // Disarms the backend's hardware, resets to IDLE, and drives the output pin back to its idle level.
  // Call this once the caller is done inspecting a terminal state (get_frame()/get_error()) to prepare
  // for the next listen()/send(). Safe to call from any state.
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
  // Only valid when has_error() is true and get_error() == DataLinkError::TIMER_ERROR. ISR backend only
  // -- always TIMER_ERROR_NONE on the RMT backend.
  TimerError get_timer_error() const { return this->timer_error_; }
  // Only valid when has_error() is true and get_error() == DataLinkError::RMT_ERROR. RMT backend only --
  // always RMT_ERROR_NONE on the ISR backend.
  RmtError get_rmt_error() const { return this->rmt_error_; }

#ifndef OPENTHERM42_DATALINK_RMT
#ifdef USE_ESP32
  static bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
#else
  static void timer_isr();
#endif
#endif

 protected:
  // Sets state_ to ERROR and records which error. Shared by both backends -- pure state, no hardware
  // dependency.
  void set_error_(DataLinkError error);
  static bool check_parity(uint32_t frame_bits);

#ifndef OPENTHERM42_DATALINK_RMT
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

  // Disarms the hardware timer only -- does NOT touch state_/error_ or the output pin. Called from inside
  // the timer ISR once a conversation reaches a terminal state (RECEIVED/SENT/ERROR), so that terminal
  // state survives for the caller to inspect via get_state()/get_error() instead of being clobbered by a
  // reset back to IDLE. Contrast with the public stop(), which is what actually resets to IDLE.
  void stop_timer_();
#else
  // Decodes a captured burst of RMT symbols (one per OT bit -- see datalink_rmt.cpp's BIT0_SYMBOL/
  // BIT1_SYMBOL) into frame_, or calls set_error_() if the Manchester/parity/stop-bit checks fail.
  // Called from the RX-done callback below.
  void decode_symbols_(const rmt_symbol_word_t *symbols, size_t word_count);
  // rmt_transmit()/rmt_receive() are both asynchronous -- these move state_ to SENT/RECEIVED (or ERROR)
  // once the hardware finishes, without blocking send()/listen() on completion the way a synchronous
  // wait would (which would just reintroduce the loop()-blocking problem this backend exists to avoid).
  static bool IRAM_ATTR tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                         void *user_ctx);
  static bool IRAM_ATTR rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                         void *user_ctx);
  // RMT only reports completion (rx_done_callback above), never "a transition just happened" -- so
  // get_state() would otherwise stay LISTENING for the whole reception instead of moving to RECEIVING
  // once the start bit arrives, unlike the ISR backend. This is a plain GPIO interrupt on in_pin_, armed
  // alongside rmt_receive() purely to catch that one edge and detach itself -- RMT still does all the
  // actual bit decoding. One interrupt per conversation, not one per sample, so it doesn't reintroduce
  // the exposure this backend exists to avoid.
  static void IRAM_ATTR start_bit_interrupt(OpenThermDataLink *self);
#endif

  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;

#ifndef OPENTHERM42_DATALINK_RMT
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;
#ifdef USE_ESP32
  gptimer_handle_t timer_handle_{nullptr};
#endif
#else
  rmt_channel_handle_t tx_channel_{nullptr};
  rmt_channel_handle_t rx_channel_{nullptr};
  rmt_encoder_handle_t copy_encoder_{nullptr};
  // One rmt_symbol_word_t per OT bit (start + 32 data/parity + stop). A class member, NOT a send()-local
  // -- rmt_transmit() is asynchronous and the copy encoder reads from this buffer for the whole ~34ms
  // transmission, well after send() itself has returned, so a stack-local array here would be read back
  // as garbage once its stack frame is reused.
  static constexpr size_t TX_SYMBOLS_COUNT = 34;
  rmt_symbol_word_t tx_symbols_[TX_SYMBOLS_COUNT];
  // One rmt_symbol_word_t per OT bit (34 = start + 32 data/parity + stop); generously sized above that
  // for noise/glitch tolerance without needing a second memory block/ping-pong refill.
  static constexpr size_t RX_SYMBOLS_CAPACITY = 64;
  rmt_symbol_word_t rx_symbols_[RX_SYMBOLS_CAPACITY];
  // One-shot timer for §4.3.1's "no start bit within response_timeout_ms" case -- see listen()/
  // initialize() in datalink_rmt.cpp for why RMT's own idle-timeout can't detect this by itself.
  esp_timer_handle_t response_timeout_handle_{nullptr};
#endif

  DataLinkState state_{DataLinkState::IDLE};
  DataLinkError error_{DataLinkError::NONE};
  TimerError timer_error_{TimerError::TIMER_ERROR_NONE};
  RmtError rmt_error_{RmtError::RMT_ERROR_NONE};

  Frame frame_;

#ifndef OPENTHERM42_DATALINK_RMT
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
#endif
};

}  // namespace esphome::opentherm42
