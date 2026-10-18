#include "epaper_spi_ssd1677_gray4.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd1677_gray4");

// A partial update reduces each pixel to its high bit: levels 2 and 3 are light, 0 and 1 dark.
void EPaperSSD1677Gray4::plane_row(size_t y, uint8_t *out) {
  const size_t src_row = y * this->row_width_;
  for (size_t i = 0; i != this->plane_row_length_(); i++)
    out[i] = gray4_plane_byte(this->buffer_[src_row + 2 * i], this->buffer_[src_row + 2 * i + 1], true);
}

// the high bit of every pixel's level goes to the new (bw) plane
// (0x24), the low bit to the old (red) plane (0x26)
bool HOT EPaperSSD1677Gray4::transfer_data() {
  if (this->is_partial_push_())
    return EPaperSSD1677::transfer_data();

  auto start_time = millis();
  const bool first_pass = this->send_red_;
  if (this->current_data_index_ == 0) {
    if (first_pass) {
      // With partial updates enabled the window follows the changed area, but the four-level
      // refresh drives every pixel from both planes, and the reset before it does not keep RAM.
      this->x_low_ = 0;
      this->x_high_ = this->width_;
      this->y_low_ = 0;
      this->y_high_ = this->height_;
      this->set_window();
    }
    this->command(first_pass ? 0x24 : 0x26);
    this->current_data_index_ = this->y_low_;
  }
  const size_t plane_row_length = (this->x_high_ - this->x_low_) / 8;
  // Stack-backed for every panel width in practice; only a custom `dimensions:` far wider than any
  // supported panel would fall back to the heap.
  SmallBufferWithHeapFallback<128> bytes_to_send_alloc(plane_row_length);
  uint8_t *bytes_to_send = bytes_to_send_alloc.get();
  ESP_LOGV(TAG, "Writing %u bytes at line %zu at %ums", plane_row_length, this->current_data_index_,
           (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    const size_t src_row = this->current_data_index_ * this->row_width_ + this->x_low_ / 4;
    for (size_t i = 0; i != plane_row_length; i++) {
      const uint8_t plane =
          gray4_plane_byte(this->buffer_[src_row + 2 * i], this->buffer_[src_row + 2 * i + 1], first_pass);
      // The OTP grayscale waveform treats data as inverted relative to monochrome
      bytes_to_send[i] = (uint8_t) ~plane;
      // What the next partial update compares against: the high bits, as a black-and-white frame.
      if (first_pass && this->sent_.is_valid())
        this->sent_[this->current_data_index_ * plane_row_length + i] = plane;
    }
    ++this->current_data_index_;
    this->write_array(bytes_to_send, plane_row_length);
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;
  if (first_pass) {
    this->send_red_ = false;
    return false;
  }
  this->send_red_ = true;
  return true;
}

void EPaperSSD1677Gray4::refresh_screen(bool partial) {
  if (this->is_partial_push_()) {
    ESP_LOGV(TAG, "Black-and-white partial refresh");
    // The border follows the LUT selected in 0x3C. The model's setting (sent with the init
    // sequence) picks the LUT that is white under the four-level waveform's inverted data; under
    // the black-and-white waveform that LUT drives black and the border darkens, so use LUT1.
    this->cmd_data(0x3C, {0x01});
    EPaperSSD1677::refresh_screen(true);
    return;
  }
  ESP_LOGV(TAG, "Four-level refresh");
  this->cmd_data(0x1A, {0x67, 0x00});  // force temperature by OTP
  this->cmd_data(0x22, {0xD7});        // four-level update sequence, panel's OTP waveform
  this->command(0x20);                 // master activation
}

}  // namespace esphome::epaper_spi
