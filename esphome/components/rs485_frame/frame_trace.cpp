#include "frame_trace.h"

#ifdef USE_RS485_FRAME_FRAME_TRACE

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstring>

namespace esphome::rs485_frame {

static const char *const TAG = "rs485_frame.trace";

void FrameTrace::init(size_t size, size_t capture_bytes, bool include_invalid) {
  this->entries_.init(size);
  this->capture_bytes_ = capture_bytes;
  this->include_invalid_ = include_invalid;
  this->hex_buf_ = std::make_unique<char[]>(format_hex_size(capture_bytes));
  this->initialized_ = true;
}

void FrameTrace::record(const uint8_t *data, size_t len, bool is_tx, bool valid, uint32_t now) {
  if (!this->initialized_ || (!valid && !this->include_invalid_))
    return;

  FrameTraceEntry *e;
  if (this->write_idx_ == this->entries_.size()) {
    // Ring is still filling for the first time -- grow it and allocate this slot's byte
    // buffer once. Every subsequent visit to this index reuses the buffer below.
    e = &this->entries_.emplace_back();
    e->init(this->capture_bytes_);
  } else {
    e = &this->entries_[this->write_idx_];
  }

  size_t captured = len < this->capture_bytes_ ? len : this->capture_bytes_;
  std::memcpy(e->bytes.get(), data, captured);
  e->captured_len = static_cast<uint8_t>(captured);
  e->total_len = len > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(len);
  e->is_tx = is_tx;
  e->valid = valid;
  e->timestamp_ms = now;

  this->write_idx_ = (this->write_idx_ + 1) % this->entries_.capacity();
}

void FrameTrace::dump() const {
  if (!this->initialized_ || this->entries_.size() == 0) {
    ESP_LOGI(TAG, "Frame trace: (empty)");
    return;
  }

  size_t n = this->entries_.size();
  // Ring not yet full: entries_ was only ever appended to in order, so index 0 is already
  // the oldest. Full: the oldest entry is the one about to be overwritten next, at write_idx_.
  size_t oldest = (n < this->entries_.capacity()) ? 0 : this->write_idx_;

  ESP_LOGI(TAG, "Frame trace (%zu of %zu frames, oldest to newest):", n, this->entries_.capacity());
  for (size_t k = 0; k < n; k++) {
    const FrameTraceEntry &e = this->entries_[(oldest + k) % n];
    format_hex_to(this->hex_buf_.get(), format_hex_size(this->capture_bytes_), e.bytes.get(), e.captured_len);
    if (e.captured_len < e.total_len) {
      ESP_LOGI(TAG, "  [%" PRIu32 "ms] %s %s len=%u (truncated to %u): %s", e.timestamp_ms, e.is_tx ? "TX" : "RX",
               e.valid ? "valid  " : "INVALID", e.total_len, e.captured_len, this->hex_buf_.get());
    } else {
      ESP_LOGI(TAG, "  [%" PRIu32 "ms] %s %s len=%u: %s", e.timestamp_ms, e.is_tx ? "TX" : "RX",
               e.valid ? "valid  " : "INVALID", e.total_len, this->hex_buf_.get());
    }
  }
}

}  // namespace esphome::rs485_frame

#endif  // USE_RS485_FRAME_FRAME_TRACE
