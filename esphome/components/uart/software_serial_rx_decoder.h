#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::uart {

/// Software serial RX decoder: on_edge() counts bits from the time between edges;
/// finalize() completes a byte whose idle high tail has no closing edge.
/// Times are in any steady tick unit.
class SoftwareSerialRxDecoder {
 public:
  static constexpr uint8_t RX_IDLE = 0xFF;

  /// Configure framing and buffer; drops all state. Follow with reset().
  void setup(uint32_t bit_ticks, uint8_t data_bits, bool parity, uint8_t stop_bits, uint8_t *buffer,
             size_t buffer_size) {
    this->data_bits_ = data_bits;
    this->stop_bit_ = data_bits + (parity ? 1 : 0);
    this->run_bits_ = this->stop_bit_ + stop_bits + 2;
    this->set_bit_ticks_(bit_ticks);
#ifdef USE_UART_AUTO_BAUD
    this->nominal_bit_ticks_ = bit_ticks;
#endif
    this->buffer_ = buffer;
    this->buffer_size_ = buffer_size;
    this->in_pos_ = 0;
    this->out_pos_ = 0;
    this->reset(0, true);
  }

#ifdef USE_UART_AUTO_BAUD
  uint32_t bit_ticks() const { return this->bit_ticks_; }
#endif

  /// Forget any partial frame; `level` is the current line level.
  void reset(uint32_t now, bool level) {
    this->bit_ = RX_IDLE;
    this->cur_byte_ = 0;
    this->last_level_ = level;
    this->last_edge_ = now;
#ifdef USE_UART_AUTO_BAUD
    this->sync_timing_ = false;
#endif
  }

  /// ISR. True to wake the loop: a byte was pushed, or the line went high in an
  /// open frame (a data 1 and the idle tail look alike) so finalize() may be needed.
  bool ESPHOME_ALWAYS_INLINE on_edge(uint32_t now, bool level) {
    const bool last_level = this->last_level_;
    // Collapsed edge
    if (level == last_level)
      return false;
    // LX106 has no divider, so count
    uint32_t delta = now - this->last_edge_;
#ifdef USE_UART_AUTO_BAUD
    // A start bit after the line idled longer than a frame
    const bool idle_start = !level && delta > this->max_run_ticks_;
    const uint8_t run_bit = this->bit_;
#endif
    if (delta > this->max_run_ticks_)
      delta = this->max_run_ticks_;
    delta += this->bit_ticks_ / 2;
    uint32_t bits = 0;
    while (delta >= this->bit_ticks_) {
      delta -= this->bit_ticks_;
      bits++;
    }
    const bool pushed = this->consume_run_(bits, last_level);
#ifdef USE_UART_AUTO_BAUD
    if (this->sync_timing_ || idle_start)
      this->auto_baud_edge_(now, last_level, run_bit, idle_start);
#endif
    this->last_edge_ = now;
    this->last_level_ = level;
    return pushed || (level && this->bit_ != RX_IDLE);
  }

  bool pending() const { return this->bit_ != RX_IDLE && this->last_level_; }

  bool finalize_due(uint32_t now) const {
    const uint8_t bit = this->bit_;
    if (bit == RX_IDLE)
      return false;
    return now - this->last_edge_ >= this->tail_ticks_(bit);
  }

  /// Call with the ISR masked.
  void finalize(uint32_t now) {
    if (!this->last_level_ || !this->finalize_due(now))
      return;
    // Remaining bits are all ones
    const uint8_t bit = this->bit_;
    uint8_t cur = this->cur_byte_;
    if (bit < this->data_bits_)
      cur |= ((1U << this->data_bits_) - 1) & ~((1U << bit) - 1);
    this->push_byte(cur);
    this->bit_ = RX_IDLE;
#ifdef USE_UART_AUTO_BAUD
    if (this->sync_timing_)
      this->end_sync_frame_(true, cur, bit + 1);
#endif
  }

  /// Drops the byte when full; holds buffer_size - 1 bytes.
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

  uint8_t *buffer() const { return this->buffer_; }
  size_t buffer_size() const { return this->buffer_size_; }

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
  void ESPHOME_ALWAYS_INLINE set_bit_ticks_(uint32_t bit_ticks) {
    this->bit_ticks_ = bit_ticks;
    this->max_run_ticks_ = bit_ticks * this->run_bits_;
  }

#ifdef USE_UART_AUTO_BAUD
  /// Runs before last_edge_ moves to `now`; the consumed run began at last_edge_ on frame bit run_bit + 1.
  void ESPHOME_ALWAYS_INLINE auto_baud_edge_(uint32_t now, bool run_level, uint8_t run_bit, bool idle_start) {
    const uint8_t bit = this->bit_;
    if (this->sync_timing_) {
      if (bit != RX_IDLE)
        return;
      // Frame ended on this run; a low stop bit is a framing error
      this->end_sync_frame_(run_level, this->cur_byte_, run_bit + 1);
    }
    if (idle_start && bit == RX_IDLE) {
      this->set_bit_ticks_(this->nominal_bit_ticks_);
      this->sync_start_ = now;
      this->sync_timing_ = true;
    }
  }

  /// `bits`: bit times from the start edge to last_edge_, the frame's last edge.
  /// Inlined into the IRAM ISR; a flash copy would fault while flash is busy.
  void ESPHOME_ALWAYS_INLINE end_sync_frame_(bool framed, uint8_t value, uint8_t bits) {
    this->sync_timing_ = false;
    // A byte ending in a long run of ones leaves too few edges to time
    if (!framed || value != UART_AUTO_BAUD_SYNC_BYTE || bits < this->data_bits_)
      return;
    this->set_bit_ticks_((this->last_edge_ - this->sync_start_ + bits / 2) / bits);
  }
#endif

  uint32_t tail_ticks_(uint8_t bit) const {
    return (this->stop_bit_ + 1 - bit) * this->bit_ticks_ + this->bit_ticks_ / 2;
  }

  bool ESPHOME_ALWAYS_INLINE consume_run_(uint32_t bits, bool level) {
    uint8_t bit = this->bit_;
    uint8_t cur = this->cur_byte_;
    bool pushed = false;
    while (bits > 0) {
      if (bit == RX_IDLE) {
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
        // Parity, not checked
        bit++;
        bits--;
      } else {
        // Stop bit; low is a framing error
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
  uint32_t bit_ticks_{0};
  uint32_t max_run_ticks_{0};
  volatile uint32_t last_edge_{0};
  uint8_t *buffer_{nullptr};
  size_t buffer_size_{0};
  volatile size_t in_pos_{0};
  size_t out_pos_{0};  // reader only
#ifdef USE_UART_AUTO_BAUD
  // Touched only by the ISR, and by setup() and finalize() with the ISR detached or masked
  uint32_t nominal_bit_ticks_{0};
  uint32_t sync_start_{0};
#endif
  /// Next frame bit (data, parity, then stop at stop_bit_) or RX_IDLE.
  volatile uint8_t bit_{RX_IDLE};
  volatile uint8_t cur_byte_{0};
  volatile bool last_level_{true};
  uint8_t data_bits_{8};
  uint8_t stop_bit_{8};
  /// Longest run counted, in bits: one more than a frame.
  uint8_t run_bits_{11};
#ifdef USE_UART_AUTO_BAUD
  bool sync_timing_{false};
#endif
};

}  // namespace esphome::uart
