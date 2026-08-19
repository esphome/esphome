#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::uart {

/// Bit timing decoder for a software serial RX pin.
///
/// on_edge() is fed from the pin change interrupt with the cycle count of the edge
/// and the level the line changed to; it counts how many bit times the previous
/// level lasted and runs them through a start/data/parity/stop state machine,
/// so the interrupt never waits for the line. Decoded bytes land in a ring buffer.
/// A byte whose trailing bits are idle high has no closing edge; the main loop
/// completes it with finalize() once enough time has passed.
///
/// Kept free of platform headers so it can be unit tested on the host. The hot
/// methods are force inlined so the platform ISR that calls them stays in IRAM.
class SoftwareSerialRxDecoder {
 public:
  static constexpr uint8_t RX_IDLE = 0xFF;

  /// Configure the framing and buffer. Drops buffered bytes and any partial frame and
  /// assumes an idle high line; call reset() afterwards with the real line level.
  void setup(uint32_t bit_cycles, uint8_t data_bits, bool parity, uint8_t stop_bits, uint8_t *buffer,
             size_t buffer_size) {
    this->bit_cycles_ = bit_cycles;
    this->data_bits_ = data_bits;
    this->stop_bit_ = data_bits + (parity ? 1 : 0);
    // Runs longer than a whole frame plus one bit are idle; cap them there.
    this->max_run_cycles_ = bit_cycles * (this->stop_bit_ + stop_bits + 2);
    this->buffer_ = buffer;
    this->buffer_size_ = buffer_size;
    this->in_pos_ = 0;
    this->out_pos_ = 0;
    this->reset(0, true);
  }

  /// Forget any partial frame; `level` is the current line level.
  void reset(uint32_t now, bool level) {
    this->bit_ = RX_IDLE;
    this->cur_byte_ = 0;
    this->last_level_ = level;
    this->last_edge_ = now;
  }

  /// ISR: the line changed to `level` at cycle `now`.
  /// Returns true when the main loop should be woken: a byte was pushed, or a frame
  /// is open and the line is high, in which case finalize() may be needed. That is
  /// deliberately conservative; a data 1 and the idle tail are indistinguishable at
  /// the edge, and repeat wakes are cheap because the wake flag is already set.
  bool ESPHOME_ALWAYS_INLINE on_edge(uint32_t now, bool level) {
    const bool last_level = this->last_level_;
    // Two edges collapsed into one interrupt: skip it so the run is still
    // measured from the last real edge and the frame stays aligned.
    if (level == last_level)
      return false;
    // Bits since the last edge, rounded to nearest; no hardware divider on the LX106, so count.
    uint32_t delta = now - this->last_edge_;
    if (delta > this->max_run_cycles_)
      delta = this->max_run_cycles_;
    delta += this->bit_cycles_ / 2;
    uint32_t bits = 0;
    while (delta >= this->bit_cycles_) {
      delta -= this->bit_cycles_;
      bits++;
    }
    const bool pushed = this->consume_run_(bits, last_level);
    this->last_edge_ = now;
    this->last_level_ = level;
    return pushed || (level && this->bit_ != RX_IDLE);
  }

  /// True while a frame is open and the line sits idle high, so a byte may be waiting on finalize().
  bool pending() const { return this->bit_ != RX_IDLE && this->last_level_; }

  /// Cheap unlocked check whether the pending byte's tail has elapsed by `now`.
  bool finalize_due(uint32_t now) const {
    const uint8_t bit = this->bit_;
    if (bit == RX_IDLE)
      return false;
    return now - this->last_edge_ >= this->tail_cycles_(bit);
  }

  /// Complete the pending byte if its tail has elapsed. Call with the ISR masked.
  void finalize(uint32_t now) {
    const uint8_t bit = this->bit_;
    if (bit == RX_IDLE || !this->last_level_ || now - this->last_edge_ < this->tail_cycles_(bit))
      return;
    this->consume_run_(this->stop_bit_ + 1 - bit, true);
  }

  /// Store a decoded byte, dropping it when the buffer is full. Also used by the start bit sampler.
  bool ESPHOME_ALWAYS_INLINE push_byte(uint8_t data) {
    size_t in = this->in_pos_;
    size_t next = in + 1;
    if (next == this->buffer_size_)
      next = 0;
    if (next == this->out_pos_)
      return false;
    this->buffer_[in] = data;
    this->in_pos_ = next;
    return true;
  }

  size_t available() const {
    // Read volatile in_pos_ once to avoid TOCTOU race with ISR.
    size_t in = this->in_pos_;
    if (in >= this->out_pos_)
      return in - this->out_pos_;
    return this->buffer_size_ - this->out_pos_ + in;
  }
  uint8_t peek_byte() const {
    if (this->in_pos_ == this->out_pos_)
      return 0;
    return this->buffer_[this->out_pos_];
  }
  uint8_t read_byte() {
    if (this->in_pos_ == this->out_pos_)
      return 0;
    uint8_t data = this->buffer_[this->out_pos_];
    size_t next = this->out_pos_ + 1;
    this->out_pos_ = next == this->buffer_size_ ? 0 : next;
    return data;
  }

 protected:
  /// Cycles the line must stay high after the last edge to hold every bit up to the first stop bit.
  uint32_t tail_cycles_(uint8_t bit) const {
    return (this->stop_bit_ + 1 - bit) * this->bit_cycles_ + this->bit_cycles_ / 2;
  }

  /// Feed `bits` consecutive bits at `level` into the frame. Returns true when a byte was pushed.
  bool ESPHOME_ALWAYS_INLINE consume_run_(uint32_t bits, bool level) {
    uint8_t bit = this->bit_;
    uint8_t cur = this->cur_byte_;
    bool pushed = false;
    while (bits > 0) {
      if (bit == RX_IDLE) {
        // Idle line, or what is left of a run after a framing error.
        if (level)
          break;
        // Start bit
        bit = 0;
        cur = 0;
        bits--;
      } else if (bit < this->data_bits_) {
        uint8_t n = this->data_bits_ - bit;
        if (n > bits)
          n = bits;
        if (level)
          cur |= ((1U << n) - 1) << bit;
        bit += n;
        bits -= n;
      } else if (bit < this->stop_bit_) {
        // Parity bit: consumed but not checked.
        bit++;
        bits--;
      } else {
        // Stop bit; a low level here is a framing error, drop the byte.
        if (level)
          pushed = this->push_byte(cur);
        bit = RX_IDLE;
        break;
      }
    }
    this->bit_ = bit;
    this->cur_byte_ = cur;
    return pushed;
  }

  // Members ordered largest to smallest to minimize padding
  uint32_t bit_cycles_{0};
  uint32_t max_run_cycles_{0};
  volatile uint32_t last_edge_{0};
  uint8_t *buffer_{nullptr};
  size_t buffer_size_{0};
  volatile size_t in_pos_{0};
  volatile size_t out_pos_{0};
  /// Index of the next frame bit after the start bit (data, then parity, then stop at stop_bit_) or RX_IDLE.
  volatile uint8_t bit_{RX_IDLE};
  volatile uint8_t cur_byte_{0};
  volatile bool last_level_{true};
  uint8_t data_bits_{8};
  uint8_t stop_bit_{8};
};

}  // namespace esphome::uart
