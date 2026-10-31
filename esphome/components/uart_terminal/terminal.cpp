#include "terminal.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::uart_terminal {

ESPHOME_LOG_TAG(TAG, "uart_terminal");

static constexpr uint8_t CRLF[] = {'\r', '\n'};
// Clear the screen and move the cursor home; the cursor-home part alone is its last three bytes
static constexpr uint8_t CLEAR_HOME[] = {0x1B, '[', '2', 'J', 0x1B, '[', 'H'};
static constexpr size_t HOME_OFFSET = 4;
// A frame that has made no progress for this long means the UART is not draining (e.g. USB CDC unplugged)
static constexpr uint32_t STALL_WARNING_MS = 5000;
// Unchanged frames are skipped, but a terminal attached later must still get the screen, and text outside
// the grid must go: every so many updates the frame goes out again, with the screen cleared first
static constexpr uint8_t FULL_REDRAW_INTERVAL = 10;

// A frame on the wire keeps the buffer; the next update waits for it and runs once the frame is out
void Terminal::update() {
  if (this->sending_) {
    this->update_pending_ = true;
    return;
  }
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
  this->tx_seq_pos_ = 0;
  this->tx_row_ = 0;
  this->tx_col_ = 0;
  this->sending_ = true;
  // A fresh start for the stall check; a pacing UART still gets one loop interval of room on the first pass
  this->last_write_ms_ = App.get_loop_component_start_time() - App.get_loop_interval();
  this->enable_loop();
}

// Writes what fits of an escape sequence or CRLF, so any room makes progress; true once all of it is out
bool Terminal::write_sequence_(const uint8_t *seq, size_t len, size_t &room) {
  const size_t n = std::min(room, len - this->tx_seq_pos_);
  this->write_array(seq + this->tx_seq_pos_, n);
  room -= n;
  this->tx_seq_pos_ += n;
  if (this->tx_seq_pos_ < len)
    return false;
  this->tx_seq_pos_ = 0;
  return true;
}

// Writes as much of the frame as the UART takes now, so a large grid or a slow line never holds the main loop
void Terminal::loop() {
  if (!this->sending_) {
    this->disable_loop();
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  size_t room = this->parent_->paced_write_room(this->last_write_ms_);
  if (room == 0) {
    if (now - this->last_write_ms_ > STALL_WARNING_MS && !this->status_has_warning())
      this->status_set_warning(LOG_STR("UART is not taking the frame"));
    return;
  }
  this->last_write_ms_ = now;
  this->status_clear_warning();
  if (this->tx_prefix_pending_) {
    // Clear the screen on a full redraw, so text outside the grid goes; otherwise only move the cursor home,
    // since every cell is rewritten
    const uint8_t *prefix = this->tx_full_ ? CLEAR_HOME : CLEAR_HOME + HOME_OFFSET;
    const size_t len = this->tx_full_ ? sizeof(CLEAR_HOME) : sizeof(CLEAR_HOME) - HOME_OFFSET;
    if (!this->write_sequence_(prefix, len, room))
      return;
    this->tx_prefix_pending_ = false;
  }
  while (room > 0 && this->tx_row_ < this->rows_) {
    if (this->tx_crlf_pending_) {
      if (!this->write_sequence_(CRLF, sizeof(CRLF), room))
        return;
      this->tx_crlf_pending_ = false;
      if (room == 0)
        return;
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
  if (this->tx_row_ == this->rows_) {
    this->sending_ = false;
    if (this->update_pending_) {
      this->update_pending_ = false;
      text_display::TextDisplay::update();
    }
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
