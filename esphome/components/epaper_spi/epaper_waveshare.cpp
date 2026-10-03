#include "epaper_waveshare.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.waveshare";

bool EpaperWaveshare::initialise(bool partial) {
  EPaperBase::initialise(partial);
  if (partial) {
    this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
    this->send_init_sequence_(this->partial_sequence_, this->partial_sequence_length_);
    this->cmd_data(0x3C, {0x80});
    this->cmd_data(0x22, {0xC0});
    this->command(0x20);
    this->next_delay_ = 100;
  } else {
    this->cmd_data(0x32, this->lut_, this->lut_length_);
    this->send_init_sequence_(this->full_sequence_, this->full_sequence_length_);
    this->cmd_data(0x3C, {0x05});
  }
  // The previous image must stay in the base RAM for partial updates to compare against,
  // so models needing it write the base RAM on full updates only.
  this->send_red_ = !(this->base_image_required_ && partial);
  return true;
}

void EpaperWaveshare::set_window() {
  this->x_low_ &= ~7;
  this->x_high_ += 7;
  this->x_high_ &= ~7;
  uint16_t x_start = this->x_low_ / 8;
  uint16_t x_end = (this->x_high_ - 1) / 8;
  this->cmd_data(0x44, {(uint8_t) x_start, (uint8_t) (x_end)});
  this->cmd_data(0x4E, {(uint8_t) x_start});
  this->cmd_data(0x45, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256), (uint8_t) (this->y_high_ - 1),
                        (uint8_t) ((this->y_high_ - 1) / 256)});
  this->cmd_data(0x4F, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256)});
  ESP_LOGV(TAG, "Set window X: %u-%u, Y: %u-%u", this->x_low_, this->x_high_, this->y_low_, this->y_high_);
}

void EpaperWaveshare::refresh_screen(bool partial) {
  if (partial) {
    this->cmd_data(0x22, {0x0F});
  } else {
    this->cmd_data(0x22, {0xC7});
  }
  this->command(0x20);
  this->next_delay_ = partial ? 100 : 3000;
}

void EpaperWaveshare::deep_sleep() {
  // Sleeping can only be left by a reset, which a partial update must avoid.
  if (this->must_keep_base_image_())
    return;
  this->cmd_data(0x10, {0x01});
}

bool EpaperWaveshare::reset() {
  // Resetting the controller restarts the alternation between the two RAM banks, so a partial
  // update would compare the new image against the wrong bank and leave stale pixels behind.
  if (this->must_keep_base_image_())
    return true;
  return EPaperMono::reset();
}
}  // namespace esphome::epaper_spi
