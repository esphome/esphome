// Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library (src/boards/Inkplate2)

#include "epaper_spi_inkplate2.h"
#include "colorconv.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate2";

// Map RGB to the panel's black/white/red via the shared converter.
enum class Inkplate2Color : uint8_t { BLACK, WHITE, RED };

static Inkplate2Color to_inkplate2_color(Color color) {
  return color_to_bwr<Inkplate2Color>(color, Inkplate2Color::BLACK, Inkplate2Color::WHITE, Inkplate2Color::RED);
}

void EPaperInkplate2::power_on() {
  // Power-on (0x04) leads the init sequence, so there is nothing to do here.
  ESP_LOGV(TAG, "Power on");
}

void EPaperInkplate2::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(0x50, {0xF7});  // VCOM and data interval
  this->command(0x02);           // power off
}

void EPaperInkplate2::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");  // full refresh only; partial is unused
  // Send 0x11 then 0x12 back-to-back: 0x11 raises busy until the refresh finishes, so waiting for idle
  // between them (as the state machine does between states) would add a ~16s stall.
  this->cmd_data(0x11, {0x00});  // stop data transfer
  this->command(0x12);           // display refresh
}

void EPaperInkplate2::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

void EPaperInkplate2::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);  // clipping active: defer to the base per-pixel path
    return;
  }

  const size_t half_buffer = this->buffer_length_ / 2;

  // Plane encoding: B/W plane 1=white, 0=black; red plane 0=red, 1=no-red.
  uint8_t bw_byte;
  uint8_t red_byte;
  switch (to_inkplate2_color(color)) {
    case Inkplate2Color::BLACK:
      bw_byte = 0x00;
      red_byte = 0xFF;
      break;
    case Inkplate2Color::RED:
      bw_byte = 0xFF;
      red_byte = 0x00;
      break;
    case Inkplate2Color::WHITE:
    default:
      bw_byte = 0xFF;
      red_byte = 0xFF;
      break;
  }

  for (size_t i = 0; i < half_buffer; i++)
    this->buffer_[i] = bw_byte;
  for (size_t i = half_buffer; i < this->buffer_length_; i++)
    this->buffer_[i] = red_byte;

  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void EPaperInkplate2::clear() { this->fill(COLOR_ON); }

void HOT EPaperInkplate2::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const size_t half_buffer = this->buffer_length_ / 2;
  const size_t pos = y * this->row_width_ + x / 8;
  const uint8_t mask = 0x80 >> (x & 0x07);  // MSB first; see fill() for plane encoding

  switch (to_inkplate2_color(color)) {
    case Inkplate2Color::BLACK:
      this->buffer_[pos] &= ~mask;
      this->buffer_[pos + half_buffer] |= mask;
      break;
    case Inkplate2Color::RED:
      this->buffer_[pos] |= mask;
      this->buffer_[pos + half_buffer] &= ~mask;
      break;
    case Inkplate2Color::WHITE:
    default:
      this->buffer_[pos] |= mask;
      this->buffer_[pos + half_buffer] |= mask;
      break;
  }
}

bool HOT EPaperInkplate2::send_buffer_range_(size_t end, uint32_t start_time) {
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;
  while (this->current_data_index_ < end) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];
    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->disable();
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME)
        return false;  // yield; resume next loop
    }
  }
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->disable();
  }
  return true;
}

bool HOT EPaperInkplate2::transfer_data() {
  const uint32_t start_time = millis();
  const size_t half_buffer = this->buffer_length_ / 2;

  // Black/white plane (first half) then red plane (second half).
  if (this->current_data_index_ == 0)
    this->command(0x10);
  if (this->current_data_index_ < half_buffer && !this->send_buffer_range_(half_buffer, start_time))
    return false;

  if (this->current_data_index_ == half_buffer)
    this->command(0x13);
  if (!this->send_buffer_range_(this->buffer_length_, start_time))
    return false;

  this->current_data_index_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
