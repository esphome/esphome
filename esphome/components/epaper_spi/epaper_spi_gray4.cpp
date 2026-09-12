#include "epaper_spi_gray4.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.gray4";

void EPaperGray4::setup() {
  EPaperBase::setup();
  this->init_shadow_();
}

// The comparison frame is only wanted where partial refresh is: with
// full_update_every at 1 every push is a full one and nothing ever diffs
// against it. The first push is always full - the counter starts at zero - so
// the frame is seeded before any partial can need it.
void EPaperGray4::init_shadow_() {
  if (!this->is_using_partial_update_())
    return;
  if (!this->shadow_.init((size_t) ((this->width_ + 7) / 8) * this->height_)) {
    ESP_LOGW(TAG, "No memory for a comparison frame; every update will be a full refresh");
  }
}

// Luminance into four even quarters. A renderer that antialiases - LVGL
// composites at 16-bit - delivers glyph edges here as real intermediate
// values, and this is where they survive instead of being thresholded.
uint8_t EPaperGray4::color_to_level_(Color color) const {
  const uint16_t sum = (uint16_t) color.r + color.g + color.b;  // 0..765
  if (sum >= 574)
    return 3;  // white
  if (sum >= 383)
    return 2;  // light gray
  if (sum >= 192)
    return 1;  // dark gray
  return 0;    // black
}

uint8_t EPaperGray4::level_at_(int x, int y) const {
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  return (this->buffer_[byte_position] >> (6 - 2 * (x % 4))) & 0x03;
}

void EPaperGray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);  // clipping active: defers to the per-pixel path
    return;
  }
  this->buffer_.fill(this->color_to_level_(color) * 0x55);  // same value in all four slots
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT EPaperGray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = 6 - 2 * (x % 4);  // most significant pixel first
  uint8_t value = this->buffer_[byte_position];
  value = (value & ~(0x03 << shift)) | (this->color_to_level_(color) << shift);
  this->buffer_[byte_position] = value;
}

bool EPaperGray4::reset() {
  if (EPaperBase::reset()) {
    // A software reset drops controller RAM, which a partial needs to keep.
    // A full push rewrites both planes anyway, so it can afford one.
    if (this->update_count_ == 0)
      this->command(0x12);
    return true;
  }
  return false;
}

void EPaperGray4::set_window_() {
  // Every push covers the whole panel: a four-level refresh drives all of
  // it from both planes, and a partial is panel-wide too - master
  // activation reads all of 0x24, a RAM window only scopes the write.
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->cmd_data(0x44, {0x00, 0x00, (uint8_t) ((this->width_ - 1) & 0xFF), (uint8_t) ((this->width_ - 1) >> 8)});
  this->cmd_data(0x4E, {0x00, 0x00});
  this->cmd_data(0x45, {0x00, 0x00, (uint8_t) ((this->height_ - 1) & 0xFF), (uint8_t) ((this->height_ - 1) >> 8)});
  this->cmd_data(0x4F, {0x00, 0x00});
}

// Both kinds of push write two planes; what differs is what goes in them.
//
//   FULL     pass 0 -> new RAM = high bit of the level
//            pass 1 -> old RAM = low bit, both inverted where the panel
//                      reads 1 as white
//   PARTIAL  pass 0 -> old RAM = the frame on the glass (the comparison)
//            pass 1 -> new RAM = the frame we want
//
// In both, the SECOND pass records what the push is putting on the glass.
// Doing it in the first would overwrite the shadow the second still has to
// read, leaving both planes identical - and then every cleared pixel is
// undriven and the old image stays underneath the new one.
bool HOT EPaperGray4::transfer_data() {
  auto start_time = millis();
  const size_t row_length = this->width_ / 8;
  const bool second_pass = this->plane_ == 1;
  if (this->current_data_index_ == 0) {
    if (!second_pass) {
      // Latch the kind of push for its whole duration: the two planes must
      // agree, and refresh_screen() must match what was written.
      this->partial_push_ = this->update_count_ != 0 && this->shadow_.is_valid();
      this->set_window_();
    }
    this->command(this->plane_command(this->partial_push_ == second_pass));
  }
  uint8_t row[128];
  this->start_data_();
  while (this->current_data_index_ != this->height_) {
    const int y = (int) this->current_data_index_;
    for (size_t b = 0; b != row_length; b++) {
      const size_t shadow_index = (size_t) y * row_length + b;
      uint8_t out, mono = 0;
      if (this->partial_push_ && !second_pass) {
        out = this->shadow_[shadow_index];  // what is on the glass
      } else {
        out = 0;
        for (uint8_t i = 0; i < 8; i++) {
          const uint8_t level = this->level_at_((int) (b * 8 + i), y);
          const uint8_t bit = this->partial_push_ ? (level >= 2) : second_pass ? (level & 1) : (level >> 1);
          out |= bit << (7 - i);
          mono |= (uint8_t) (level >= 2) << (7 - i);
        }
        if (!this->partial_push_ && this->gray_planes_inverted())
          out = (uint8_t) ~out;
      }
      row[b] = out;
      if (second_pass && this->shadow_.is_valid())
        this->shadow_[shadow_index] = mono;
    }
    ++this->current_data_index_;
    this->write_array(row, row_length);
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;  // come back next loop
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  if (!second_pass) {
    this->plane_ = 1;
    return false;  // second plane next time round
  }
  this->plane_ = 0;
  return true;
}

// The base class offers its own full/partial opinion from a counter, but the
// planes were written against partial_push_, latched when the push began -
// it can differ, because a partial is downgraded when there is no comparison
// frame yet. The waveform has to match what was actually written.
void EPaperGray4::refresh_screen(bool partial) {
  if (this->partial_push_) {
    ESP_LOGV(TAG, "Partial refresh");
    this->refresh_partial();
  } else {
    ESP_LOGV(TAG, "Four-level refresh");
    this->refresh_gray();
  }
}

void EPaperGray4::refresh_partial() {
  this->cmd_data(0x22, {0xFF});  // OTP display mode 2, with temperature
  this->command(0x20);           // master activation
}

// The panel sleeps after every push, as it does for a monochrome display -
// it wakes on the reset the next push performs anyway, so this costs nothing
// and saves its idle draw for however long the host stays up.
//
// Mode 1 keeps RAM across the sleep and that reset. This class writes both
// planes every time and so does not depend on it, but the deeper mode buys
// nothing measurable while the host is awake, and mode 1 is the one with
// hardware evidence behind it on this family.
//
// Mode 2 is for the host going to sleep: the rail is usually cut behind it,
// so there is nothing to preserve and no wake to pay for.
void EPaperGray4::deep_sleep() {
  if (this->sleep_panel_deep_) {
    this->sleep_panel_deep_ = false;
    ESP_LOGV(TAG, "Panel deep sleep, mode 2");
    this->cmd_data(0x10, {0x03});
    return;
  }
  ESP_LOGV(TAG, "Panel deep sleep, mode 1");
  this->cmd_data(0x10, {0x01});
}

// Vendor waveform selection: 0xD7 is not a named row of the datasheet's
// Table 7-1. Seeed's dashboard driver and the stock firmware both send it,
// preceded by the forced OTP temperature.
void EPaperStickyGray4::refresh_gray() {
  this->cmd_data(0x1A, {0x67, 0x00});  // force temperature by OTP
  this->cmd_data(0x22, {0xD7});        // four-level update sequence
  this->command(0x20);                 // master activation
}

}  // namespace esphome::epaper_spi
