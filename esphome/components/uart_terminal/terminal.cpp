#include "terminal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::uart_terminal {

ESPHOME_LOG_TAG(TAG, "uart_terminal");

static constexpr uint8_t CRLF[] = {'\r', '\n'};
// Unchanged frames are skipped, but a terminal attached later must still get the screen: every so many
// updates the frame goes out again, with the screen cleared first
static constexpr uint8_t FULL_REDRAW_INTERVAL = 10;

void HOT Terminal::display() {
  const size_t size = this->rows_ * this->columns_;
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < size; i++)
    hash = fnv1a_hash_extend(hash, this->buffer_[i]);
  const bool full = this->updates_until_redraw_ == 0;
  if (!full && hash == this->last_frame_hash_) {
    this->updates_until_redraw_--;
    return;
  }
  this->last_frame_hash_ = hash;
  this->updates_until_redraw_ = FULL_REDRAW_INTERVAL - 1;  // this frame counts as the first of the interval

  // Clear the screen on a full redraw, so text outside the grid goes; otherwise only move the cursor home,
  // since every cell is rewritten
  this->write_str(full ? "\x1B[2J\x1B[H" : "\x1B[H");
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
