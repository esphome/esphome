#include "uart_bridge.h"

#include "esphome/core/application.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::uart {

static const char *const TAG = "uart.bridge";

static constexpr uint32_t US_PER_SEC = 1000000;
// A frame ends after 3.5 quiet characters (here in half characters), and after at least 1750 us above 19200 baud,
// as in Modbus RTU and the modbus hub.
static constexpr uint32_t FRAME_GAP_HALF_CHARS = 7;
static constexpr uint32_t MIN_FRAME_GAP_US = 1750;
// The modbus hub's wait for the rest of a frame from a UART that hands over bytes in chunks.
static constexpr uint32_t CHUNKED_FRAME_GAP_US = 50000;
// A destination that took nothing for this long holds up the source; its block is dropped.
static constexpr uint32_t STALL_DROP_US = 1000000;
static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
// Normal loop passes come every 16 ms. Closer to a deadline than this, or while a FIFO is fed, pass at full speed.
static constexpr uint32_t FAST_WAIT_US = 20000;

static uint32_t bits_per_char(UARTComponent *uart) {
  // A zero means the setter was never called; fall back to 8N1 like the modbus hub.
  const uint32_t data_bits = uart->get_data_bits() != 0 ? uart->get_data_bits() : 8;
  const uint32_t stop_bits = uart->get_stop_bits() != 0 ? uart->get_stop_bits() : 1;
  return 1 + data_bits + (uart->get_parity() == UART_CONFIG_PARITY_NONE ? 0 : 1) + stop_bits;
}

// Wire time of one character in us, rounded up; 0 when the UART has no baud rate.
static uint32_t char_time_us(UARTComponent *uart) {
  const uint32_t baud = uart->get_baud_rate();
  return baud == 0 ? 0 : (bits_per_char(uart) * US_PER_SEC + baud - 1) / baud;
}

static uint32_t gap_after(uint32_t char_us) {
  return std::max(MIN_FRAME_GAP_US, (char_us * FRAME_GAP_HALF_CHARS + 1) / 2);
}

static uint32_t remaining(uint32_t elapsed, uint32_t total) { return elapsed < total ? total - elapsed : 0; }

void UARTBridgePipe::setup() {
  if (!this->from_pushes_) {
    const uint32_t char_us = char_time_us(this->from_);
    if (!this->from_wire_ || char_us == 0) {
      this->gap_us_ = CHUNKED_FRAME_GAP_US;
    } else {
      this->gap_us_ = gap_after(char_us);
      // ESP-IDF moves bytes out of the FIFO once more than rx_full_threshold are in, or after rx_timeout quiet
      // characters (ESP32: about 2.4 for 2). After a full batch, the next part can take this long to show up.
      const size_t threshold = this->from_->get_rx_full_threshold();
      if (threshold != UARTComponent::RX_FULL_THRESHOLD_UNSET) {
        this->batch_bytes_ = static_cast<uint16_t>(threshold);
        this->batch_us_ =
            static_cast<uint32_t>(threshold + 1 + this->from_->get_rx_timeout()) * char_us + this->gap_us_;
      }
    }
  }
  const uint32_t to_char_us = char_time_us(this->to_);
  if (this->to_wire_ && to_char_us != 0) {
    this->to_baud_ = this->to_->get_baud_rate();
    this->to_bits_ = static_cast<uint8_t>(bits_per_char(this->to_));
    this->to_gap_us_ = gap_after(to_char_us);
  }
}

void UARTBridgePipe::send_(uint32_t now_us) {
  if (this->block_len_ == 0) {
    return;
  }
  if (!this->to_->is_connected()) {
    this->drop_(this->len_, LOG_STR("Destination not connected"));
    this->clear_();
    return;
  }
  // A block that starts a frame waits until the line has been quiet for a frame gap.
  if (this->sent_ == 0 && this->new_frame_ && now_us - this->tx_start_us_ < this->tx_busy_us_ + this->to_gap_us_) {
    return;
  }
  size_t n = this->block_len_ - this->sent_;
  if (!this->to_virtual_) {
    // A UART that cannot report its room (SIZE_MAX) takes the whole block in one write_array(), as the modbus hub
    // writes a frame: no gap inside the block, but the call blocks until all but its FIFO's worth is on the wire.
    n = std::min(n, this->to_->available_for_write());
  }
  if (n == 0) {
    // A FIFO frees room within a character; one that frees none for this long holds up the source.
    this->stalled_ = now_us - this->progress_us_ >= FAST_WAIT_US;
    return;
  }
  this->to_->write_array(this->buf_ + this->sent_, n);
  // The line sends these bytes after whatever it still has: from now, or from the end of that.
  if (now_us - this->tx_start_us_ >= this->tx_busy_us_) {
    this->tx_start_us_ = now_us;
    this->tx_busy_us_ = 0;
  }
  if (this->to_baud_ != 0) {
    this->tx_busy_us_ += (static_cast<uint32_t>(n) * this->to_bits_ * US_PER_SEC + this->to_baud_ - 1) / this->to_baud_;
  }
  this->sent_ += static_cast<uint16_t>(n);
  this->progress_us_ = now_us;
  this->stalled_ = false;
  if (this->sent_ < this->block_len_) {
    return;
  }
  // A pushed block waiting behind this one is next.
  this->len_ -= this->block_len_;
  std::memmove(this->buf_, this->buf_ + this->block_len_, this->len_);
  this->block_len_ = this->len_;
  this->sent_ = 0;
}

bool UARTBridgePipe::receive_(uint32_t now_us) {
  if (!this->from_pushes_ && this->block_len_ == 0) {
    this->collect_(now_us);
  }
  if (this->stalled_ && now_us - this->progress_us_ >= STALL_DROP_US) {
    this->drop_(this->len_, LOG_STR("Destination stalled"));
    this->clear_();
  }
  const bool busy = this->len_ != 0;
  uint32_t wait = 0;
  if (this->block_len_ == 0) {
    wait = remaining(now_us - this->last_rx_us_, this->batched_ ? this->batch_us_ : this->gap_us_);
  } else if (this->sent_ == 0 && this->new_frame_) {
    wait = remaining(now_us - this->tx_start_us_, this->tx_busy_us_ + this->to_gap_us_);
  }
  if (busy && !this->stalled_ && wait < FAST_WAIT_US) {
    this->fast_.start();
  } else {
    this->fast_.stop();
  }
  return busy;
}

void UARTBridgePipe::collect_(uint32_t now_us) {
  if (!this->to_->is_connected()) {
    // Read and drop, so a later connection does not get stale bytes.
    size_t dropped = this->len_;
    for (size_t have = this->from_->available(); have != 0;) {
      const size_t n = std::min(have, BLOCK_SIZE);
      if (!this->from_->read_array(this->buf_, n)) {
        break;
      }
      dropped += n;
      have -= n;
    }
    this->len_ = 0;
    if (dropped != 0) {
      this->drop_(dropped, LOG_STR("Destination not connected"));
    }
    return;
  }
  const size_t have = this->from_->available();
  if (have != 0) {
    const size_t n = std::min(have, BLOCK_SIZE - this->len_);
    if (!this->from_->read_array(this->buf_ + this->len_, n)) {
      return;
    }
    this->len_ += static_cast<uint16_t>(n);
    this->last_rx_us_ = now_us;
    this->batched_ = this->batch_bytes_ != 0 && n >= this->batch_bytes_;
  }
  if (this->len_ == 0 ||
      (this->len_ < BLOCK_SIZE && now_us - this->last_rx_us_ < (this->batched_ ? this->batch_us_ : this->gap_us_))) {
    return;
  }
  this->block_len_ = this->len_;
  // A block cut at BLOCK_SIZE goes on in the next one, which follows without a gap.
  this->new_frame_ = !this->cut_;
  this->cut_ = this->len_ == BLOCK_SIZE;
  this->progress_us_ = now_us;
  this->send_(now_us);
}

void UARTBridgePipe::on_block(const uint8_t *data, size_t len) {
  if (this->to_virtual_) {
    this->to_->write_array(data, len);
    return;
  }
  // One block goes out and one more may wait behind it, each kept whole.
  if (this->len_ != this->block_len_ || this->len_ + len > BLOCK_SIZE) {
    this->drop_(len, LOG_STR("Destination busy"));
    return;
  }
  const uint32_t now_us = micros();
  std::memcpy(this->buf_ + this->len_, data, len);
  if (this->len_ == 0) {
    this->block_len_ = static_cast<uint16_t>(len);
    this->progress_us_ = now_us;
  }
  this->len_ += static_cast<uint16_t>(len);
  this->send_(now_us);
  this->receive_(now_us);
}

void UARTBridgePipe::clear_() {
  this->len_ = 0;
  this->block_len_ = 0;
  this->sent_ = 0;
  this->stalled_ = false;
}

void UARTBridgePipe::drop_(size_t len, const LogString *why) {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->drop_log_ms_ != 0 && now - this->drop_log_ms_ < DROP_LOG_INTERVAL_MS) {
    return;
  }
  // A zero stamp would read as "never logged".
  this->drop_log_ms_ = now == 0 ? 1 : now;
  ESP_LOGW(TAG, "%s, dropped %zu bytes", LOG_STR_ARG(why), len);
}

void UARTBridge::setup() {
  this->a_to_b_.setup();
  this->b_to_a_.setup();
  if (!this->a_to_b_.needs_loop() && !this->b_to_a_.needs_loop()) {
    this->disable_loop();
  }
}

void UARTBridge::loop() {
  this->a_to_b_.poll();
  this->b_to_a_.poll();
}

void UARTBridge::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Bridge:\n"
                "  A: %s, frame gap %" PRIu32 " us\n"
                "  B: %s, frame gap %" PRIu32 " us",
                this->a_to_b_.from_pushes() ? LOG_STR_LITERAL("virtual") : LOG_STR_LITERAL("polled"),
                this->a_to_b_.frame_gap_us(),
                this->b_to_a_.from_pushes() ? LOG_STR_LITERAL("virtual") : LOG_STR_LITERAL("polled"),
                this->b_to_a_.frame_gap_us());
}

}  // namespace esphome::uart
