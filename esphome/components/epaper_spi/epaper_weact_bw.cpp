#include "epaper_weact_bw.h"

namespace esphome::epaper_spi {

bool EPaperWeActBW::reset() {
  // Keep controller RAM and diff state intact for partial updates.
  if (this->update_count_ != 0) {
    return true;
  }
  return EPaperBase::reset();
}

bool EPaperWeActBW::initialise(bool partial) {
  // Full update starts a new cycle: wake/reset/init the controller.
  // Partial updates keep panel state and do not re-run init sequence.
  this->current_partial_update_ = partial;
  if (!partial) {
    EPaperBase::initialise(false);
  }
  this->send_red_ = true;
  return true;
}

bool HOT EPaperWeActBW::transfer_data() {
  // Always transfer the full screen buffer, but for partial updates, the controller 
  // will only apply the diff to the changed area.
  // Using dirty-rect windows for partial updates causes stale 0x24 data outside the
  // dirty area, which breaks the per-pixel diff that drives partial refresh.
  const bool full_cycle_update = !this->current_partial_update_;
  if (this->current_data_index_ == 0) {
    // Set full-screen window and cursor.
    const uint8_t x_end = (uint8_t) (this->row_width_ - 1);
    const uint16_t y_end = (uint16_t) (this->height_ - 1);
    this->cmd_data(0x44, {0x00, x_end});
    this->cmd_data(0x45, {0x00, 0x00, (uint8_t) (y_end & 0xFF), (uint8_t) (y_end >> 8)});
    this->cmd_data(0x4E, {0x00});
    this->cmd_data(0x4F, {0x00, 0x00});
    // Full cycle: pass 1 = 0x24 (new frame), pass 2 = 0x26 (old frame seed).
    // Partial: write 0x24 (new frame) only; 0x26 is updated post-refresh in power_off().
    if (full_cycle_update) {
      this->command(this->send_red_ ? 0x24 : 0x26);
    } else {
      this->command(0x24);
    }
  }

  const uint32_t start_time = millis();
  this->start_data_();
  while (this->current_data_index_ < this->height_) {
    for (uint16_t x = 0; x < this->row_width_; ++x) {
      this->write_byte((uint8_t) ~this->buffer_[(size_t) this->current_data_index_ * this->row_width_ + x]);
    }
    ++this->current_data_index_;
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;

  if (full_cycle_update && this->send_red_) {
    this->send_red_ = false;
    return false;  // Trigger second pass for 0x26.
  }
  this->send_red_ = false;
  return true;
}

void EPaperWeActBW::refresh_screen(bool partial) {
  this->cmd_data(0x4E, {0x00});
  this->cmd_data(0x4F, {0x00, 0x00});

  if (partial) {
    // partial update sequence
    this->cmd_data(0x3C, {0x80});
    this->cmd_data(0x21, {0x00, 0x00});
    this->cmd_data(0x22, {0xFC});
  } else {
    // full update sequence
    this->cmd_data(0x3C, {0x01});
    this->cmd_data(0x21, {0x40, 0x00});
    this->cmd_data(0x1A, {0x6E});
    this->cmd_data(0x22, {0xD7});
  }
  this->command(0x20);
}

void EPaperWeActBW::power_on() {
  // Only power on at the start of a full-update cycle.
  if (this->current_partial_update_) {
    return;
  }
  this->cmd_data(0x22, {0xF8});
  this->command(0x20);
}

void EPaperWeActBW::power_off() {
  if (this->update_count_ == 0) {
    // Full-cycle boundary (cycle just completed): power off.
    this->cmd_data(0x22, {0x83});
    this->command(0x20);
    return;
  }

  if (!this->current_partial_update_) {
    // Full base update in progress (cycle start): stay awake, do not power down,
    // and do not overwrite OLD buffer here.
    return;
  }

  // Partial update completed: sync the OLD frame buffer (0x26) so the next
  // partial update diffs against the frame just displayed, not the original
  // full-cycle seed.
  const uint8_t x_end = (uint8_t) (this->row_width_ - 1);
  const uint16_t y_end = (uint16_t) (this->height_ - 1);
  this->cmd_data(0x44, {0x00, x_end});
  this->cmd_data(0x45, {0x00, 0x00, (uint8_t) (y_end & 0xFF), (uint8_t) (y_end >> 8)});
  this->cmd_data(0x4E, {0x00});
  this->cmd_data(0x4F, {0x00, 0x00});
  this->command(0x26);
  this->start_data_();
  for (uint16_t y = 0; y < this->height_; ++y) {
    for (uint16_t x = 0; x < this->row_width_; ++x) {
      this->write_byte((uint8_t) ~this->buffer_[(size_t) y * this->row_width_ + x]);
    }
  }
  this->disable();
}

void EPaperWeActBW::deep_sleep() {
  // Only deep sleep when the full update cycle wraps.
  if (this->update_count_ != 0) {
    return;
  }
  this->cmd_data(0x10, {0x01});
}

}  // namespace esphome::epaper_spi