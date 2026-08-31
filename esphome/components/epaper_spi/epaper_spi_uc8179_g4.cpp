#include "epaper_spi_uc8179_g4.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.uc8179_g4";

// DTM plane bits by 2-bit gray level, per Waveshare's EPD_7in5_V2 4-gray
// sample: level 3 (white) -> (1,1), level 0 (black) -> (0,0).
static const uint8_t LEVEL_BITS_DTM1[4] = {0, 0, 1, 1};
static const uint8_t LEVEL_BITS_DTM2[4] = {0, 1, 0, 1};
// 1-bit plane packing for the differential partial and fast-flip paths, and
// its complement (which happens to equal LEVEL_BITS_DTM1). NOTE: the bit
// polarity, the KW/WK register-LUT slot assignment (see the model file) and
// the 0x50=0xA9 border setting were validated TOGETHER on hardware; changing
// any one of them in isolation inverts or smears the image.
static const uint8_t PLANE_BIT[4] = {1, 1, 0, 0};
static const uint8_t PLANE_BIT_INV[4] = {0, 0, 1, 1};

void EPaperUC8179G4::setup() {
  EPaperBase::setup();
  if (this->is_failed())
    return;
  const size_t plane_len = this->buffer_length_ / 2u;
  if (!this->prev_plane_.init(plane_len)) {
    ESP_LOGW(TAG, "Could not allocate %zu bytes for the previous-frame plane; partial refresh disabled", plane_len);
  }
}

void EPaperUC8179G4::fill(Color color) {
  // If clipping is active, fall back to the base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  const uint8_t level = luminance_to_level_(color);
  this->buffer_.fill(level * 0b01010101u);  // 4 identical 2-bit pixels per byte
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT EPaperUC8179G4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t level = luminance_to_level_(color);
  const uint32_t pos = x + y * this->width_;
  const size_t byte_position = pos >> 2;
  const uint8_t shift = 6 - 2 * (pos & 0x3);
  this->buffer_[byte_position] = (this->buffer_[byte_position] & ~(0x3 << shift)) | (level << shift);
}

uint8_t HOT EPaperUC8179G4::pack_byte_(size_t i, const uint8_t bit_for_level[4]) {
  uint8_t out = 0;
  for (size_t j = 0; j != 2; j++) {
    const uint8_t b = this->buffer_[i + j];
    for (int k = 3; k >= 0; k--) {
      out = (out << 1) | bit_for_level[(b >> (2 * k)) & 0x3];
    }
  }
  return out;
}

bool EPaperUC8179G4::initialise(bool partial) {
  // Decide this update's mode. A requested fast flip wins; a partial needs
  // both the cadence slot (partial) and a stored previous frame.
  if (this->fast_flip_) {
    this->mode_ = Mode::FAST_FLIP;
    this->fast_flip_ = false;
  } else if (partial && this->prev_valid_) {
    this->mode_ = Mode::PARTIAL;
  } else {
    this->mode_ = Mode::FULL;
  }

  // The static init sequence configures the FULL 4-gray mode (PSR=0x1F,
  // CDI=0x10/0x07, 4-gray OTP waveform) and uploads the partial LUTs.
  EPaperBase::initialise(partial);

  switch (this->mode_) {
    case Mode::FULL:
      break;  // already configured by the init sequence
    case Mode::FAST_FLIP:
      this->cmd_data(CMD_VCOM_INTERVAL, {0xA9, 0x07});
      this->cmd_data(CMD_WAVEFORM_CTRL, {0x5A});  // OTP fast 1-bit waveform
      break;
    case Mode::PARTIAL: {
      this->cmd_data(CMD_PANEL_SETTING, {0x3F});  // KW mode, LUT from register
      this->cmd_data(CMD_VCOM_INTERVAL, {0xA9, 0x07});
      this->command(CMD_PARTIAL_IN);
      const uint16_t xe = this->width_ - 1;
      const uint16_t ye = this->height_ - 1;
      // Full-screen partial window: the differential LUTs make unchanged
      // pixels a physical no-op, so windowing adds nothing but bookkeeping.
      this->cmd_data(CMD_PARTIAL_WINDOW,
                     {0x00, 0x00, static_cast<uint8_t>(xe >> 8), static_cast<uint8_t>(xe & 0xFF), 0x00, 0x00,
                      static_cast<uint8_t>(ye >> 8), static_cast<uint8_t>(ye & 0xFF), 0x01});
      break;
    }
  }
  return true;
}

bool HOT EPaperUC8179G4::transfer_data() {
  const uint32_t start_time = millis();
  const size_t plane_len = this->buffer_length_ / 2u;
  const bool prev_ok = this->prev_plane_.is_valid();
  uint8_t chunk[MAX_TRANSFER_SIZE];

  // Phase 1: DTM1 (0x10) — the "old data" plane.
  // FULL: gray plane 1. FAST_FLIP: complement of the 1-bit plane.
  // PARTIAL: the stored previous frame's plane, verbatim.
  if (this->current_data_index_ < plane_len) {
    if (this->current_data_index_ == 0) {
      this->command(CMD_DTM1);
    }
    this->start_data_();
    while (this->current_data_index_ < plane_len) {
      const size_t n = std::min(MAX_TRANSFER_SIZE, plane_len - this->current_data_index_);
      for (size_t i = 0; i != n; i++) {
        const size_t idx = this->current_data_index_ + i;
        if (this->mode_ == Mode::PARTIAL) {
          chunk[i] = this->prev_plane_[idx];
        } else {
          chunk[i] = this->pack_byte_(idx * 2, this->mode_ == Mode::FULL ? LEVEL_BITS_DTM1 : PLANE_BIT_INV);
        }
      }
      this->write_array(chunk, n);
      this->current_data_index_ += n;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->disable();
        return false;
      }
    }
    this->disable();
  }

  // Phase 2: DTM2 (0x13) — the "new data" plane.
  // FULL: gray plane 2. FAST_FLIP / PARTIAL: the current 1-bit plane, which
  // is also stored as it streams to become "old data" for the next partial.
  if (this->current_data_index_ < 2 * plane_len) {
    if (this->current_data_index_ == plane_len) {
      this->command(CMD_DTM2);
    }
    this->start_data_();
    while (this->current_data_index_ < 2 * plane_len) {
      const size_t n = std::min(MAX_TRANSFER_SIZE, 2 * plane_len - this->current_data_index_);
      for (size_t i = 0; i != n; i++) {
        const size_t idx = this->current_data_index_ + i - plane_len;
        if (this->mode_ == Mode::FULL) {
          chunk[i] = this->pack_byte_(idx * 2, LEVEL_BITS_DTM2);
        } else {
          chunk[i] = this->pack_byte_(idx * 2, PLANE_BIT);
          if (prev_ok) {
            this->prev_plane_[idx] = chunk[i];
          }
        }
      }
      this->write_array(chunk, n);
      this->current_data_index_ += n;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->disable();
        return false;
      }
    }
    this->disable();
  }

  // Phase 3 (FULL only, no SPI): rebuild the stored plane from the buffer so
  // the first partial after a full refresh diffs against reality.
  if (this->mode_ == Mode::FULL && prev_ok) {
    while (this->current_data_index_ < 3 * plane_len) {
      const size_t idx = this->current_data_index_ - 2 * plane_len;
      this->prev_plane_[idx] = this->pack_byte_(idx * 2, PLANE_BIT);
      ++this->current_data_index_;
      if ((idx & 0xFF) == 0xFF && millis() - start_time > MAX_TRANSFER_TIME) {
        return false;
      }
    }
  }

  this->current_data_index_ = 0;
  this->prev_valid_ = prev_ok;
  return true;
}

void EPaperUC8179G4::power_on() {
  this->command(CMD_POWER_ON);
  this->next_delay_ = 100;  // give BUSY time to assert before it is polled
}

void EPaperUC8179G4::refresh_screen(bool partial) {
  ESP_LOGD(TAG, "Refreshing (%s)",
           this->mode_ == Mode::FULL      ? "full 4-gray"
           : this->mode_ == Mode::PARTIAL ? "differential partial"
                                          : "fast flip");
  this->command(CMD_REFRESH);
  this->next_delay_ = 100;
}

void EPaperUC8179G4::power_off() { this->command(CMD_POWER_OFF); }

void EPaperUC8179G4::deep_sleep() {
  // Safe in every mode: each update fully re-initialises the controller and
  // the previous frame's plane is kept host-side, so no controller state
  // needs to survive.
  this->cmd_data(CMD_DEEP_SLEEP, {0xA5});
}

}  // namespace esphome::epaper_spi
