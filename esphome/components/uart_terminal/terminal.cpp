#include "terminal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::uart_terminal {

ESPHOME_LOG_TAG(TAG, "uart_terminal");

static constexpr uint8_t CRLF[] = {'\r', '\n'};

void HOT Terminal::display() {
  const size_t size = this->rows_ * this->columns_;
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < size; i++)
    hash = fnv1a_hash_extend(hash, this->buffer_[i]);
  if (this->shown_ && hash == this->last_frame_hash_)
    return;
  this->last_frame_hash_ = hash;
  this->shown_ = true;

  // Move the cursor home; every cell is rewritten, so no clear is needed
  this->write_str("\x1B[H");
  for (uint8_t row = 0; row < this->rows_; row++) {
    this->write_array(&this->buffer_[row * this->columns_], this->columns_);
    if (row + 1 < this->rows_)
      this->write_array(CRLF, sizeof(CRLF));
  }
}

void Terminal::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Terminal:\n"
                "  Dimensions: %ux%u",
                this->columns_, this->rows_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::uart_terminal
