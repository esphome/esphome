#pragma once

#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_timer.h"
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
  // The RMT peripheral driver returned an error -- see RmtError for which operation failed.
  RMT_ERROR,
};

const char *data_link_error_to_string(DataLinkError error);

// Which specific RMT driver call failed when DataLinkError::RMT_ERROR is reported.
enum class RmtError : uint8_t {
  RMT_ERROR_NONE = 0,
  RMT_ERROR_NEW_TX_CHANNEL,
  RMT_ERROR_NEW_RX_CHANNEL,
  RMT_ERROR_NEW_ENCODER,
  RMT_ERROR_ENABLE,
  RMT_ERROR_TRANSMIT,
  RMT_ERROR_RECEIVE,
  RMT_ERROR_TIMER,
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
// Implemented on the ESP32 RMT peripheral (datalink_rmt.cpp, on variants with RMT hardware): a 34-bit
// OT frame is 34 RMT symbols, which fits in a single RMT memory block -- the peripheral clocks the whole
// frame autonomously once started, with no per-sample CPU/ISR involvement, so a long interrupt-mask
// elsewhere (e.g. another component's InterruptLock) can't corrupt an in-flight frame.
class OpenThermDataLink {
 public:
  OpenThermDataLink(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  // Configures the pins and the RMT channels. Returns false if that setup failed -- check get_error()
  // (and get_rmt_error() for the specific operation) for why.
  bool initialize();

  // Starts listening for a response frame from the boiler. response_timeout_ms bounds how long to wait
  // for the response's start bit (§4.3.1: 20-400 ms is the legal range for a compliant boiler) before
  // reporting DataLinkError::RESPONSE_TIMEOUT.
  void listen(uint32_t response_timeout_ms);

  // Starts transmitting a frame. The parity bit (§4.2.1) is computed and set automatically.
  void send(const Frame &frame);

  // Disarms the hardware, resets to IDLE, and drives the output pin back to its idle level.
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
  // Only valid when has_error() is true and get_error() == DataLinkError::RMT_ERROR.
  RmtError get_rmt_error() const { return this->rmt_error_; }

 protected:
  // Sets state_ to ERROR and records which error -- pure state, no hardware dependency.
  void set_error_(DataLinkError error);
  static bool check_parity(uint32_t frame_bits);

#ifdef USE_ESP32
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
  // once the start bit arrives. This is a plain GPIO interrupt on in_pin_, armed
  // alongside rmt_receive() purely to catch that one edge and detach itself -- RMT still does all the
  // actual bit decoding. One interrupt per conversation, not one per sample, so it doesn't reintroduce
  // the interrupt-mask exposure RMT exists to avoid.
  static void IRAM_ATTR start_bit_interrupt(OpenThermDataLink *self);
  // Aborts a receive that is still armed by disabling and re-enabling the RX channel. Logs if that
  // fails, since the channel then stays unusable and every later receive would fail without saying why.
  void restart_rx_channel_();
#endif

  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;

#ifdef USE_ESP32
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
  RmtError rmt_error_{RmtError::RMT_ERROR_NONE};

  Frame frame_;
};

}  // namespace esphome::opentherm42
