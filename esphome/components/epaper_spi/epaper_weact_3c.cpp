#include "epaper_weact_3c.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_weact_3c");

void EPaperWeAct3C::set_window() {
  // For full screen refresh, we always start from (0,0)
  // The y_low_/y_high_ values track the dirty region for optimization,
  // but for display refresh we need to write from the beginning
  uint16_t x_start = 0;
  uint16_t x_end = this->width_ - 1;
  uint16_t y_start = 0;
  uint16_t y_end = this->height_ - 1;  // height = 296 for 2.9" display

  // Set RAM X address boundaries (0x44)
  // X coordinates are byte-aligned (divided by 8)
  this->cmd_data(0x44, {(uint8_t) (x_start / 8), (uint8_t) (x_end / 8)});

  // Set RAM Y address boundaries (0x45)
  // Format: Y start (LSB, MSB), Y end (LSB, MSB)
  this->cmd_data(0x45, {(uint8_t) y_start, (uint8_t) (y_start >> 8), (uint8_t) (y_end & 0xFF), (uint8_t) (y_end >> 8)});

  // Reset RAM X counter to start (0x4E) - 1 byte
  this->cmd_data(0x4E, {(uint8_t) (x_start / 8)});

  // Reset RAM Y counter to start (0x4F) - 2 bytes (LSB, MSB)
  this->cmd_data(0x4F, {(uint8_t) y_start, (uint8_t) (y_start >> 8)});
}

bool HOT EPaperWeAct3C::transfer_data() {
  const uint32_t start_time = millis();
  const size_t half_buffer = this->buffer_length_ / 2u;

  // current_data_index_ is the position in buffer_: 0 before the push starts, then the red plane
  // [half_buffer, buffer_length_), sent first, then the black/white plane (0, half_buffer).
  // A push only yields after sending some data, so it never stops at exactly 0 or half_buffer.
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "transfer_data: sending RED buffer (0x26)");
    this->set_window();  // Reset RAM X/Y counters to start position
    this->command(0x26);
    this->current_data_index_ = half_buffer;
  }
  if (this->current_data_index_ >= half_buffer) {
    if (!this->send_buffer_range_(this->buffer_length_, this->invert_red_ ? 0xFF : 0x00, start_time))
      return false;
    ESP_LOGV(TAG, "transfer_data: finished red buffer, sending BLACK buffer (0x24)");
    // Do NOT reset RAM counters here for WeAct displays (Reference implementation behavior)
    this->command(0x24);
    this->current_data_index_ = 0;
  }
  if (!this->send_buffer_range_(half_buffer, 0xFF, start_time))  // the panel wants 1=white
    return false;

  this->current_data_index_ = 0;
  ESP_LOGV(TAG, "transfer_data: completed (red=%u, black=%u bytes)", half_buffer, half_buffer);
  return true;
}

void EPaperWeAct3C::refresh_screen(bool partial) {
  // SSD1680 refresh sequence:
  // Reset RAM X/Y address counters to 0,0 so display reads from start
  // 0x4E: RAM X counter - 1 byte (X / 8)
  // 0x4F: RAM Y counter - 2 bytes (Y LSB, Y MSB)
  this->cmd_data(0x4E, {0x00});        // RAM X counter = 0 (1 byte)
  this->cmd_data(0x4F, {0x00, 0x00});  // RAM Y counter = 0 (2 bytes)

  // Send UPDATE_FULL command (0x22) with display update control parameter
  // Both WeAct and waveshare reference use 0xF7: {0x22, 0xF7}
  // 0xF7 = Display update: Load temperature, Load LUT, Enable RAM content
  this->cmd_data(0x22, {0xF7});  // Command 0x22 with parameter 0xF7
  this->command(0x20);           // Activate display update

  // COMMAND TERMINATE FRAME READ WRITE (required by SSD1680)
  // Removed 0xFF based on working reference implementation
  // this->command(0xFF);
}

void EPaperWeAct3C::power_on() {
  // Power on sequence - send command to turn on power
  // According to SSD1680 spec: 0x22, 0xF8 powers on the display
  this->cmd_data(0x22, {0xF8});  // Power on
  this->command(0x20);           // Activate
}

void EPaperWeAct3C::power_off() {
  // Power off sequence - send command to turn off power
  // According to SSD1680 spec: 0x22, 0x83 powers off the display
  this->cmd_data(0x22, {0x83});  // Power off
  this->command(0x20);           // Activate
}

void EPaperWeAct3C::deep_sleep() {
  // Deep sleep sequence
  this->cmd_data(0x10, {0x01});  // Deep sleep mode
}

}  // namespace esphome::epaper_spi
