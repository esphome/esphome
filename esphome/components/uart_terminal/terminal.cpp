#include "terminal.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome::uart_terminal {

ESPHOME_LOG_TAG(TAG, "uart_terminal");

static constexpr uint8_t CRLF[] = {'\r', '\n'};
// Unchanged frames are skipped, but a terminal attached later must still get the screen, and text outside
// the grid must go: every so many updates the frame goes out again, with the screen cleared first
static constexpr uint8_t FULL_REDRAW_INTERVAL = 10;

// A frame on the wire keeps the buffer; the next update waits for it
void Terminal::update() {
  if (!this->sending_)
    text_display::TextDisplay::update();
}

void HOT Terminal::display() {
  if (this->buffer_ == nullptr)
    return;
  const size_t size = this->rows_ * this->columns_;
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < size; i++)
    hash = fnv1a_hash_extend(hash, this->buffer_[i]);
  // The countdown runs on every update, changed or not, so a frame that changes every time still gets
  // its screen clear every tenth update
  const bool full = this->updates_until_redraw_ == 0;
  this->updates_until_redraw_ = full ? FULL_REDRAW_INTERVAL - 1 : this->updates_until_redraw_ - 1;
  if (!full && hash == this->last_frame_hash_)
    return;
  this->last_frame_hash_ = hash;

  this->tx_full_ = full;
  this->tx_prefix_pending_ = true;
  this->tx_crlf_pending_ = false;
  this->tx_row_ = 0;
  this->tx_col_ = 0;
  this->sending_ = true;
  this->enable_loop();
}

// Writes as much of the frame as the UART takes now, so a large grid or a slow line never holds the main loop
void Terminal::loop() {
  if (!this->sending_) {
    this->disable_loop();
    return;
  }
  size_t room = this->parent_->paced_write_room(this->last_write_ms_);
  if (room == 0)
    return;
  this->last_write_ms_ = App.get_loop_component_start_time();
  if (this->tx_prefix_pending_) {
    // Clear the screen on a full redraw, so text outside the grid goes; otherwise only move the cursor home,
    // since every cell is rewritten. The sequence goes out whole
    const char *prefix = this->tx_full_ ? "\x1B[2J\x1B[H" : "\x1B[H";
    const size_t len = strlen(prefix);
    if (room < len)
      return;
    this->write_str(prefix);
    room -= len;
    this->tx_prefix_pending_ = false;
  }
  while (room > 0 && this->tx_row_ < this->rows_) {
    if (this->tx_crlf_pending_) {
      if (room < sizeof(CRLF))
        return;
      this->write_array(CRLF, sizeof(CRLF));
      room -= sizeof(CRLF);
      this->tx_crlf_pending_ = false;
    }
    const size_t n = std::min<size_t>(room, this->columns_ - this->tx_col_);
    this->write_array(&this->buffer_[this->tx_row_ * this->columns_ + this->tx_col_], n);
    this->tx_col_ += n;
    room -= n;
    if (this->tx_col_ == this->columns_) {
      this->tx_col_ = 0;
      this->tx_row_++;
      this->tx_crlf_pending_ = this->tx_row_ < this->rows_;  // CRLF between rows only
    }
  }
  if (this->tx_row_ == this->rows_)
    this->sending_ = false;
}

void Terminal::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Terminal:\n"
                "  Dimensions: %ux%u",
                this->columns_, this->rows_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::uart_terminal
