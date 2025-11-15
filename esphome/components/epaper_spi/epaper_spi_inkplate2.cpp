// Reference implementation:
// https://github.com/SolderedElectronics/Inkplate-Arduino-library

#include "epaper_spi_inkplate2.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate2";
static constexpr size_t MAX_TRANSFER_SIZE = 128;

void EPaperInkplate2::power_on() {
  // DEVIATION: Unlike some e-paper displays, Inkplate 2 seems to require the power-on command (0x04)
  // BEFORE data transfer can occur. This matches Soldered's reference implementation.
  // See: https://github.com/SolderedElectronics/Inkplate-Arduino-library/blob/master/src/boards/Inkplate2.cpp#L197
  // This empty function maintains compatibility with the epaper_spi state machine.
  ESP_LOGD(TAG, "Power on (already powered during init)");
}

void EPaperInkplate2::power_off() {
  ESP_LOGD(TAG, "Power off");
  this->command(0x50);  // VCOM and data interval setting
  this->data(0xF7);
  this->command(0x02);  // Power  EPD off
}

void EPaperInkplate2::refresh_screen() {
  ESP_LOGV(TAG, "Refresh screen");
  // DEVIATION: Sending 0x11 at the end of transfer_data() triggers the busy pin, which won't clear until
  // after refresh completes. The state machine waits for idle between states, causing a 16+ second
  // delay. Sending 0x11 and 0x12 consecutively avoids this wait, matching Soldered's implementation.
  // See: https://github.com/SolderedElectronics/Inkplate-Arduino-library/blob/master/src/boards/Inkplate2.cpp#L150-L153
  this->command(0x11);  // Stop data transfer
  this->data(0x00);
  this->command(0x12);     // Display refresh
  delayMicroseconds(500);  // Required by hardware - wait at least 200μs
}

void EPaperInkplate2::deep_sleep() {
  ESP_LOGD(TAG, "Deep sleep");
  this->command(0x07);  // Put EPD in deep sleep
  this->data(0xA5);
}

void EPaperInkplate2::fill(Color color) {
  const uint32_t buf_half_len = this->buffer_length_ / 2;

  // Hardware encoding: B&W: 0=white, 1=black; Red: 0=red, 1=no red
  bool is_red = (color.red > 200) && (color.green < 100) && (color.blue < 100);
  bool is_black = !color.is_on();

  uint8_t bw_byte, red_byte;

  if (is_black) {
    // Black: B&W buffer = 0xFF (all black), Red buffer = 0xFF (no red)
    bw_byte = 0xFF;
    red_byte = 0xFF;
  } else if (is_red) {
    // Red: B&W buffer = 0x00 (all white), Red buffer = 0x00 (all red)
    bw_byte = 0x00;
    red_byte = 0x00;
  } else {
    // White: B&W buffer = 0x00 (all white), Red buffer = 0xFF (no red)
    bw_byte = 0x00;
    red_byte = 0xFF;
  }

  // Fill B/W buffer (first half)
  for (uint32_t i = 0; i < buf_half_len; i++) {
    this->buffer_[i] = bw_byte;
  }

  // Fill Red buffer (second half)
  for (uint32_t i = buf_half_len; i < this->buffer_length_; i++) {
    this->buffer_[i] = red_byte;
  }
}

void EPaperInkplate2::clear() {
  // Clear to white, like real paper
  this->fill(COLOR_ON);
}

void HOT EPaperInkplate2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return;

  const uint32_t buf_half_len = this->buffer_length_ / 2;
  const uint32_t pos = (x + y * this->width_) / 8;
  const uint8_t subpos = x & 0x07;
  const uint8_t bit_position = 7 - subpos;  // MSB first
  const uint8_t mask = 1 << bit_position;

  bool is_red = (color.red > 200) && (color.green < 100) && (color.blue < 100);
  bool is_black = !color.is_on();

  // Update B&W buffer
  if (is_black) {
    this->buffer_[pos] |= mask;  // Black: set bit to 1
  } else {
    this->buffer_[pos] &= ~mask;  // White or red: clear bit to 0
  }

  // Update Red buffer
  if (is_red) {
    this->buffer_[pos + buf_half_len] &= ~mask;  // Red: clear bit to 0
  } else {
    this->buffer_[pos + buf_half_len] |= mask;  // No red: set bit to 1
  }
}

bool HOT EPaperInkplate2::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buf_half_len = this->buffer_length_ / 2;

  if (this->current_data_index_ == 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->transfer_start_time_ = millis();
#endif
    ESP_LOGV(TAG, "Start sending B/W data at %ums", (unsigned) millis());
    this->command(0x10);  // Start B/W data transfer
  }

  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // Transfer B/W buffer (first half)
  while (this->current_data_index_ < buf_half_len) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->end_data_();
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Come back next loop
        return false;
      }
    }
  }

  // Flush remaining B/W data
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->end_data_();
    buf_idx = 0;
  }

  // Start Red buffer transfer
  if (this->current_data_index_ == buf_half_len) {
    ESP_LOGV(TAG, "Start sending Red data at %ums", (unsigned) millis());
    this->command(0x13);  // Start Red data transfer
  }

  // Transfer Red buffer (second half)
  while (this->current_data_index_ < this->buffer_length_) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->end_data_();
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Come back next loop
        return false;
      }
    }
  }

  // Flush remaining Red data
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->end_data_();
  }

  this->current_data_index_ = 0;
  ESP_LOGV(TAG, "Sent all data in %" PRIu32 " ms", millis() - this->transfer_start_time_);
  return true;
}

}  // namespace esphome::epaper_spi
