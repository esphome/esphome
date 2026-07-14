#include "epaper_waveshare_2p9_v2.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.waveshare_2p9_v2";

bool EpaperWaveshare2P9V2::initialise(bool partial) {
  if (!partial) {
    // Full update: OTP waveform only (legacy r2), no registered full LUT.
    this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
    this->write_prev_plane_ = true;
    this->transferring_prev_plane_ = false;
    return true;
  }

  // Partial path matches WaveshareEPaper2P9InV2R2::display() (SDK Rev 2.1).
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(this->reset_duration_);  // NOLINT
    this->reset_pin_->digital_write(true);
    delay(this->reset_duration_);  // NOLINT
  }

  this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
  this->cmd_data(0x37, {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00});
  this->cmd_data(0x3C, {0x80});
  this->cmd_data(0x22, {0xC0});
  this->command(0x20);
  this->next_delay_ = 100;
  this->write_prev_plane_ = false;
  this->transferring_prev_plane_ = false;
  return true;
}

void EpaperWaveshare2P9V2::refresh_screen(bool partial) {
  if (partial) {
    this->cmd_data(0x22, {0x0F});
  } else {
    this->cmd_data(0x22, {0xF7});
  }
  this->command(0x20);
  this->next_delay_ = partial ? 100 : 3000;
}

bool HOT EpaperWaveshare2P9V2::transfer_data() {
  auto start_time = millis();
  if (this->current_data_index_ == 0) {
    this->set_window();
    // First pass: 0x24 (BW). Optional second pass: 0x26 with the same buffer (base image).
    this->command(this->transferring_prev_plane_ ? 0x26 : 0x24);
    this->current_data_index_ = this->y_low_;
  }

  size_t row_length = (this->x_high_ - this->x_low_) / 8;
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(row_length);
  ESP_LOGV(TAG, "Writing %u bytes at line %zu at %ums", row_length, this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    size_t data_idx = this->current_data_index_ * this->row_width_ + this->x_low_ / 8;
    for (size_t i = 0; i != row_length; i++) {
      bytes_to_send[i] = this->buffer_[data_idx++];
    }
    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), row_length);  // NOLINT
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;

  if (!this->transferring_prev_plane_ && this->write_prev_plane_) {
    this->transferring_prev_plane_ = true;
    return false;
  }
  this->transferring_prev_plane_ = false;
  this->write_prev_plane_ = false;
  return true;
}

}  // namespace esphome::epaper_spi
