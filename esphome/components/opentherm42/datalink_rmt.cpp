#include "datalink.h"

#ifdef OPENTHERM42_DATALINK_RMT

#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED

#include "esp_err.h"
#include "esp_timer.h"
#include "esphome/core/log.h"

namespace esphome::opentherm42 {

ESPHOME_LOG_TAG(TAG, "opentherm42.datalink_rmt");

// §3.3.2: 1ms nominal bit period, split into two 500µs Manchester half-bits. For TX, one
// rmt_symbol_word_t (two levels + two durations) is emitted per OT bit -- a 34-bit frame (start + 32
// data/parity + stop) is 34 symbols, comfortably inside a single RMT memory block (this component
// requests 64), so the hardware clocks the whole frame with no per-symbol CPU/ISR involvement, unlike
// the ISR backend's 200µs sampling loop. RX is NOT a fixed 34 symbols, though -- see decode_symbols_()'s
// comment for why.
static constexpr uint32_t HALF_BIT_US = 500;
// §3.3.2's own two tolerance windows, taken directly from the spec diagram/text rather than the ISR
// backend's 200µs-tick-granularity approximation of them (its capture_ > 0xF/0xFF checks, equivalent to
// 800/1600µs, were a sampling-resolution compromise the 1µs-resolution RMT backend doesn't need to
// inherit):
// - MID_BIT: the mandatory mid-bit transition, 500µs nominal, -100/+150µs per the diagram's acceptance
//   window. Applies when a bit boundary's optional transition occurs, measured from that boundary.
// - FULL_BIT: the bit period, 1ms nominal, -10%/+15% (text, §3.3.2). Applies when the boundary
//   transition is skipped (two same-level half-bits merge into one observed segment measured from the
//   previous mandatory edge).
// Every transition is one or the other -- Manchester structure doesn't allow a transition anywhere else
// in a valid frame, so anything landing outside both windows is a genuine protocol error.
static constexpr uint32_t MID_BIT_MIN_US = 400;
static constexpr uint32_t MID_BIT_MAX_US = 650;
static constexpr uint32_t FULL_BIT_MIN_US = 900;
static constexpr uint32_t FULL_BIT_MAX_US = 1150;
// Generous idle-timeout for detecting the end of a received frame -- comfortably above the longest
// legitimate in-frame gap (FULL_BIT_MAX_US) but well below the multi-millisecond silence that follows a
// real stop bit. Needs verification against real hardware, same as the rest of this backend.
static constexpr uint32_t RX_IDLE_TIMEOUT_NS = 2000000;  // 2ms

OpenThermDataLink::OpenThermDataLink(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin)
    : in_pin_(in_pin), out_pin_(out_pin) {}

// bit=1: first half LOW, second half HIGH (active-to-idle, §3.3.1). bit=0: the reverse. Matches the ISR
// backend's write_bit_() polarity exactly -- level0/level1 get XORed with out_pin_->is_inverted() below,
// the same way remote_transmitter_rmt.cpp handles pin inversion for RMT (the hardware's own
// flags.invert_out is left at 0; RMT doesn't know about ESPHome's pin-level inversion setting).
static rmt_symbol_word_t bit_symbol(bool bit_value, bool inverted) {
  rmt_symbol_word_t symbol{};
  bool const level0 = (bit_value ? 0 : 1) ^ inverted;
  bool const level1 = (bit_value ? 1 : 0) ^ inverted;
  symbol.duration0 = HALF_BIT_US;
  symbol.level0 = level0;
  symbol.duration1 = HALF_BIT_US;
  symbol.level1 = level1;
  return symbol;
}

bool OpenThermDataLink::initialize() {
  this->in_pin_->pin_mode(gpio::FLAG_INPUT);
  this->in_pin_->setup();
  this->out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->out_pin_->setup();
  this->out_pin_->digital_write(true);  // idle level

  rmt_tx_channel_config_t const tx_config = {
      .gpio_num = gpio_num_t(this->out_pin_->get_pin()),
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,  // 1 µs per tick
      .mem_block_symbols = 64,
      .trans_queue_depth = 1,  // one frame in flight at a time
      .flags = {.invert_out = 0},
  };
  if (rmt_new_tx_channel(&tx_config, &this->tx_channel_) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_NEW_TX_CHANNEL;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_new_tx_channel failed");
    return false;
  }

  rmt_copy_encoder_config_t const copy_config{};
  if (rmt_new_copy_encoder(&copy_config, &this->copy_encoder_) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_NEW_ENCODER;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_new_copy_encoder failed");
    return false;
  }

  rmt_tx_event_callbacks_t const tx_callbacks = {.on_trans_done = OpenThermDataLink::tx_done_callback};
  if (rmt_tx_register_event_callbacks(this->tx_channel_, &tx_callbacks, this) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_ENABLE;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_tx_register_event_callbacks failed");
    return false;
  }

  if (rmt_enable(this->tx_channel_) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_ENABLE;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_enable (tx) failed");
    return false;
  }

  rmt_rx_channel_config_t const rx_config = {
      .gpio_num = gpio_num_t(this->in_pin_->get_pin()),
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,
      .mem_block_symbols = 64,
      .flags = {.invert_in = 0},
  };
  if (rmt_new_rx_channel(&rx_config, &this->rx_channel_) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_NEW_RX_CHANNEL;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_new_rx_channel failed");
    return false;
  }

  rmt_rx_event_callbacks_t const rx_callbacks = {.on_recv_done = OpenThermDataLink::rx_done_callback};
  if (rmt_rx_register_event_callbacks(this->rx_channel_, &rx_callbacks, this) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_ENABLE;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_rx_register_event_callbacks failed");
    return false;
  }

  if (rmt_enable(this->rx_channel_) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_ENABLE;
    this->error_ = DataLinkError::RMT_ERROR;
    this->state_ = DataLinkState::ERROR;
    ESP_LOGE(TAG, "rmt_enable (rx) failed");
    return false;
  }

  // §4.3.1's 20-400ms "did the boiler ever start responding" window has no RMT-native equivalent: the
  // RMT receiver's own idle-timeout (signal_range_max_ns) only measures gaps *between* captured edges,
  // so it never fires if no edge arrives at all. This one-shot timer is armed once per listen() call and
  // cancelled the instant rx_done_callback fires for any reason -- unlike the ISR backend's 200µs
  // recurring sample tick, it only ever needs to fire once per conversation (and only in the timeout
  // case), so it carries essentially none of the interrupt-collision exposure the ISR backend has.
  esp_timer_create_args_t const timer_args = {
      .callback =
          [](void *arg) {
            auto *self = static_cast<OpenThermDataLink *>(arg);
            if (self->state_ == DataLinkState::LISTENING) {
              self->set_error_(DataLinkError::RESPONSE_TIMEOUT);
              self->in_pin_->detach_interrupt();
              rmt_disable(self->rx_channel_);
              rmt_enable(self->rx_channel_);
            }
          },
      .arg = this,
      .name = "ot_rx_timeout",
  };
  esp_timer_create(&timer_args, &this->response_timeout_handle_);

  return true;
}

void OpenThermDataLink::listen(uint32_t response_timeout_ms) {
  this->state_ = DataLinkState::LISTENING;

  rmt_receive_config_t const receive_config = {
      .signal_range_min_ns = 1000,
      .signal_range_max_ns = RX_IDLE_TIMEOUT_NS,
  };
  if (rmt_receive(this->rx_channel_, this->rx_symbols_, sizeof(this->rx_symbols_), &receive_config) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_RECEIVE;
    this->set_error_(DataLinkError::RMT_ERROR);
    return;
  }

  // See start_bit_interrupt()/datalink.h's comment -- purely for an accurate get_state(), RMT does all
  // the real decoding regardless of whether this fires.
  this->in_pin_->attach_interrupt(OpenThermDataLink::start_bit_interrupt, this, gpio::INTERRUPT_RISING_EDGE);

  esp_timer_start_once(this->response_timeout_handle_, static_cast<uint64_t>(response_timeout_ms) * 1000ULL);
}

void OpenThermDataLink::send(const Frame &frame) {
  // §4.2: P(1) MSG-TYPE(3) SPARE(4) | DATA-ID(8) | DATA-VALUE(16), MSB-first.
  uint32_t tx_data = (static_cast<uint32_t>(frame.type) << 28) | (static_cast<uint32_t>(frame.id) << 16) |
                     (static_cast<uint32_t>(frame.value_hb) << 8) | frame.value_lb;
  if (!check_parity(tx_data)) {
    tx_data |= 0x80000000;  // set P so the total '1' count across the 32 bits is even
  }

  bool const inverted = this->out_pin_->is_inverted();
  this->tx_symbols_[0] = bit_symbol(true, inverted);  // start bit
  for (uint8_t i = 0; i < 32; i++) {
    this->tx_symbols_[1 + i] = bit_symbol(((tx_data >> (31 - i)) & 1) != 0, inverted);
  }
  this->tx_symbols_[33] = bit_symbol(true, inverted);  // stop bit

  this->state_ = DataLinkState::SENDING;

  // eot_level sets the raw GPIO level the hardware drives once transmission finishes -- left at its
  // zero-initialized default (LOW) this stays LOW between every conversation instead of returning to the
  // idle-HIGH level stop() otherwise maintains, which hides the idle-to-active edge that marks the start
  // of the NEXT transmission from the boiler (the line is already low when that transmission's own first
  // half starts low too, so there's no visible transition there at all). Must match the same
  // inversion-aware mapping bit_symbol() uses, not just a bare 1.
  rmt_transmit_config_t const transmit_config = {.flags = {.eot_level = inverted ? 0u : 1u}};
  if (rmt_transmit(this->tx_channel_, this->copy_encoder_, this->tx_symbols_, sizeof(this->tx_symbols_),
                   &transmit_config) != ESP_OK) {
    this->rmt_error_ = RmtError::RMT_ERROR_TRANSMIT;
    this->set_error_(DataLinkError::RMT_ERROR);
  }
}

void OpenThermDataLink::stop() {
  esp_timer_stop(this->response_timeout_handle_);
  this->in_pin_->detach_interrupt();
  this->state_ = DataLinkState::IDLE;
  this->out_pin_->digital_write(true);  // idle level
}

void IRAM_ATTR OpenThermDataLink::start_bit_interrupt(OpenThermDataLink *self) {
  self->in_pin_->detach_interrupt();  // one-shot -- RMT handles every subsequent edge
  if (self->state_ == DataLinkState::LISTENING) {
    self->state_ = DataLinkState::RECEIVING;
  }
}

bool IRAM_ATTR OpenThermDataLink::tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                                   void *user_ctx) {
  auto *self = static_cast<OpenThermDataLink *>(user_ctx);
  if (self->state_ == DataLinkState::SENDING) {
    self->state_ = DataLinkState::SENT;
  }
  return false;
}

bool IRAM_ATTR OpenThermDataLink::rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                                   void *user_ctx) {
  auto *self = static_cast<OpenThermDataLink *>(user_ctx);
  esp_timer_stop(self->response_timeout_handle_);
  self->decode_symbols_(edata->received_symbols, edata->num_symbols);
  return false;
}

namespace {
// RMT packs two (level, duration) segments per rmt_symbol_word_t. Fetches the k-th (0-based) segment
// across the whole array; returns false once there are no more (k past the end, or an unpaired final
// slot the driver marks with duration 0).
bool get_segment(const rmt_symbol_word_t *symbols, size_t word_count, size_t k, bool *level, uint32_t *duration_us) {
  size_t const word_idx = k / 2;
  if (word_idx >= word_count) {
    return false;
  }
  const rmt_symbol_word_t &word = symbols[word_idx];
  if ((k % 2) == 0) {
    if (word.duration0 == 0) {
      return false;
    }
    *level = static_cast<bool>(word.level0);
    *duration_us = word.duration0;
  } else {
    if (word.duration1 == 0) {
      return false;
    }
    *level = static_cast<bool>(word.level1);
    *duration_us = word.duration1;
  }
  return true;
}
}  // namespace

// RMT records one segment per actual electrical transition, not one per bit: Manchester's mandatory
// mid-bit transition always produces an edge, but the transition at a bit *boundary* is optional --
// skipped whenever two adjacent bits happen to share the same phase, in which case one segment spans
// both halves (~2x the nominal half-bit duration) instead of two separate ~1x segments. So the number of
// captured segments varies with the data pattern; it cannot be assumed to be a fixed 2*34.
//
// Each segment is classified against exactly one of the two §3.3.2 windows, chosen by what the *previous*
// transition was (tracked by `clock`, matching the ISR backend's on_timer_tick_ semantics):
// - clock==1 (the last transition was a boundary, or the start bit's own rising edge): the next
//   transition MUST be the mandatory mid-bit edge, within MID_BIT_MIN..MID_BIT_MAX of it. Nothing else is
//   valid here -- Manchester guarantees the mandatory edge, so an out-of-window gap is a frame error.
// - clock==0 (the last transition was a mandatory sample): the next transition is either the optional
//   boundary (MID_BIT window, not a sample) or, if that boundary was skipped, this bit's own merged
//   mandatory edge arriving directly a full bit later (FULL_BIT window, a sample). A gap matching neither
//   window is a frame error.
void OpenThermDataLink::decode_symbols_(const rmt_symbol_word_t *symbols, size_t word_count) {
  bool const inverted = this->in_pin_->is_inverted();
  uint32_t data = 0;
  uint8_t bit_pos = 0;
  // 1 = expecting the mandatory mid-bit edge next. The first captured segment is the gap from the start
  // bit's rising edge (detected by arming listen(), analogous to the ISR's LISTENING state) to the next
  // transition -- i.e. the start bit's own mandatory mid-bit edge -- exactly mirroring the ISR's
  // capture_=1/clock_=1 reset "as if the start bit was already captured".
  uint8_t clock = 1;

  for (size_t k = 0;; k++) {
    bool level;
    uint32_t duration_us;
    if (!get_segment(symbols, word_count, k, &level, &duration_us)) {
      // Ran out of captured segments before the stop bit was classified -- the line went quiet (RMT's
      // own idle-timeout) before the frame finished.
      this->set_error_(DataLinkError::MANCHESTER_TIMEOUT);
      return;
    }
    level = level ^ inverted;

    bool is_sample_point;
    if (clock == 1) {
      if (duration_us < MID_BIT_MIN_US || duration_us > MID_BIT_MAX_US) {
        this->set_error_(duration_us > FULL_BIT_MAX_US ? DataLinkError::MANCHESTER_TIMEOUT
                                                       : DataLinkError::MANCHESTER_NO_TRANSITION);
        return;
      }
      is_sample_point = true;
    } else if (duration_us >= MID_BIT_MIN_US && duration_us <= MID_BIT_MAX_US) {
      is_sample_point = false;  // the optional boundary transition arrived on time -- not a sample
    } else if (duration_us >= FULL_BIT_MIN_US && duration_us <= FULL_BIT_MAX_US) {
      is_sample_point = true;  // boundary was skipped; this is the next bit's own merged mandatory edge
    } else {
      this->set_error_(duration_us > FULL_BIT_MAX_US ? DataLinkError::MANCHESTER_TIMEOUT
                                                     : DataLinkError::MANCHESTER_NO_TRANSITION);
      return;
    }

    if (!is_sample_point) {
      clock = 1;  // boundary transition -- wait for the next mandatory edge
      continue;
    }

    // Valid sample point. `level` is the level held during the segment that just ended, taken directly
    // as the bit value -- matching the ISR backend's record_bit_(last): no inversion beyond the
    // pin-level one above (RX and TX use different raw-level<->bit-value conventions on this hardware
    // bridge; see write_bit_() in datalink.cpp for the TX side's, which is NOT the same mapping).
    if (bit_pos == 33) {
      // 33 data+parity bits already captured, the same as the ISR backend's bit_pos_: the very first of
      // those 33 samples is actually the start bit's own mandatory mid-bit edge (always '1'), not part of
      // the 32-bit payload -- it gets silently dropped by plain uint32_t overflow once 32 more bits have
      // been shifted in after it, leaving exactly the 32-bit frame behind. This transition is the stop
      // bit.
      if (!level) {
        this->set_error_(DataLinkError::INVALID_STOP_BIT);
        return;
      }
      if (!check_parity(data)) {
        this->set_error_(DataLinkError::PARITY_ERROR);
        return;
      }
      this->frame_.type = (data >> 28) & 0x7;
      this->frame_.id = (data >> 16) & 0xFF;
      this->frame_.value_hb = (data >> 8) & 0xFF;
      this->frame_.value_lb = data & 0xFF;
      this->state_ = DataLinkState::RECEIVED;
      return;
    }
    data = (data << 1) | (level ? 1 : 0);
    bit_pos++;
    clock = 0;
  }
}

}  // namespace esphome::opentherm42

#endif  // SOC_RMT_SUPPORTED
#endif  // OPENTHERM42_DATALINK_RMT
