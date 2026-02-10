#include "epaper_weact_3c.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_weact_3c";

// SSD1683 3-color display notes:
// - Buffer uses 1 bit per pixel, 8 pixels per byte
// - Buffer first half (black_offset): Black/White plane (1=black, 0=white)
// - Buffer second half (red_offset): Red plane (1=red, 0=no red)
// - Total buffer: width * height / 4 bytes = 2 * (width * height / 8)
// - For 128x296: 128*296/4 = 9472 bytes total (4736 per color)

void EPaperWeAct3C::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  // Calculate position in the 1-bit buffer
  const uint32_t pos = (x + y * this->width_) / 8u;
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  // Use luminance threshold for B/W mapping
  // Split at halfway point (382 = (255*3)/2)
  bool is_white = (static_cast<int>(color.r) + color.g + color.b) > 382;

  // Update black/white plane (first half of buffer)
  if (is_white) {
    // White pixel - clear bit in black plane
    this->buffer_[pos] &= ~bit;
  } else {
    // Black pixel - set bit in black plane
    this->buffer_[pos] |= bit;
  }

  // Update red plane (second half of buffer)
  // Red if red component is dominant (r > g+b)
  if (color.r > color.g + color.b) {
    // Red pixel - set bit in red plane
    this->buffer_[red_offset + pos] |= bit;
  } else {
    // Not red - clear bit in red plane
    this->buffer_[red_offset + pos] &= ~bit;
  }
}

void EPaperWeAct3C::fill(Color color) {
  // For 3-color e-paper with 1-bit buffer format:
  // - Black buffer: 1=black, 0=white
  // - Red buffer: 1=red, 0=no red
  // The buffer is stored as two halves: [black plane][red plane]
  const size_t half_buffer = this->buffer_length_ / 2u;

  // Use luminance threshold for B/W mapping
  bool is_white = (static_cast<int>(color.r) + color.g + color.b) > 382;
  bool is_red = color.r > color.g + color.b;

  // Fill both planes
  if (is_white) {
    // White - both planes = 0x00
    this->buffer_.fill(0x00);
  } else if (is_red) {
    // Red - black plane = 0x00, red plane = 0xFF
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0x00;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0xFF;
  } else {
    // Black - black plane = 0xFF, red plane = 0x00
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0xFF;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0x00;
  }
}

void EPaperWeAct3C::clear() {
  // Clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

bool HOT EPaperWeAct3C::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  const size_t half_buffer = buffer_length / 2u;

  ESP_LOGV(TAG, "transfer_data: buffer_length=%u, half_buffer=%u", buffer_length, half_buffer);

  // Use a local buffer for SPI transfers
  static constexpr size_t MAX_TRANSFER_SIZE = 128;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // First, send the RED buffer (0x26 = WRITE_COLOR)
  // The red plane is in the second half of our buffer
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "transfer_data: sending RED buffer (0x26)");
    this->command(0x26);
  }

  size_t red_offset = half_buffer;
  while (this->current_data_index_ < half_buffer) {
    size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, half_buffer - this->current_data_index_);

    for (size_t i = 0; i < bytes_to_copy; i++) {
      bytes_to_send[i] = this->buffer_[red_offset + this->current_data_index_ + i];
    }

    this->start_data_();
    this->write_array(bytes_to_send, bytes_to_copy);
    this->disable();

    this->current_data_index_ += bytes_to_copy;

    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      return false;
    }
  }

  // Finished the red buffer, now send the BLACK buffer (0x24 = WRITE_BLACK)
  // The black plane is in the first half of our buffer
  if (this->current_data_index_ == half_buffer) {
    ESP_LOGV(TAG, "transfer_data: finished red buffer, sending BLACK buffer (0x24)");

    this->command(0x24);
    this->current_data_index_ = 0;
  }

  while (this->current_data_index_ < half_buffer) {
    size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, half_buffer - this->current_data_index_);

    for (size_t i = 0; i < bytes_to_copy; i++) {
      bytes_to_send[i] = this->buffer_[this->current_data_index_ + i];
    }

    this->start_data_();
    this->write_array(bytes_to_send, bytes_to_copy);
    this->disable();

    this->current_data_index_ += bytes_to_copy;

    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      return false;
    }
  }

  this->current_data_index_ = 0;
  ESP_LOGV(TAG, "transfer_data: completed (red=%u, black=%u bytes)", half_buffer, half_buffer);
  return true;
}

void EPaperWeAct3C::refresh_screen(bool partial) {
  // SSD1683 refresh sequence:
  // 1. Send UPDATE_FULL command (0x22) with display update control (0xF7 = enable RAM content, enable bypass mode)
  // 2. Send ACTIVATE command (0x20) to start the display update
  this->command(0x22);
  this->cmd_data(0xF7, {0xF7});  // Display update control
  this->command(0x20);           // Activate display update
}

void EPaperWeAct3C::power_on() {
  // Power on sequence is handled in initialize()
}

void EPaperWeAct3C::power_off() {
  // Power off sequence
}

void EPaperWeAct3C::deep_sleep() {
  // Deep sleep sequence
  this->command(0x10);
  this->cmd_data(0x01, {0x01});  // Deep sleep mode
}

}  // namespace esphome::epaper_spi
