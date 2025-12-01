#include "epaper_spi_spectra_e6.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.6c";
static constexpr unsigned char GRAY_THRESHOLD = 50;

enum E6Color {
  BLACK,
  WHITE,
  YELLOW,
  RED,
  SKIP_1,
  BLUE,
  GREEN,
  CYAN,
  SKIP_2,
};

static uint8_t color_to_hex(Color color) {
  // --- Step 1: Check for Grayscale (Black or White) ---
  // We define "grayscale" as a color where the min and max components
  // are close to each other.
  unsigned char max_rgb = std::max({color.r, color.g, color.b});
  unsigned char min_rgb = std::min({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < GRAY_THRESHOLD) {
    // It's a shade of gray. Map to BLACK or WHITE.
    // We split the luminance at the halfway point (382 = (255*3)/2)
    if ((static_cast<int>(color.r) + color.g + color.b) > 382) {
      return WHITE;
    }
    return BLACK;
  }
  // --- Step 2: Check for Primary/Secondary Colors ---
  // If it's not gray, it's a color. We check which components are
  // "on" (over 128) vs "off". This divides the RGB cube into 8 corners.
  bool r_on = (color.r > 128);
  bool g_on = (color.g > 128);
  bool b_on = (color.b > 128);

  if (r_on && g_on && !b_on) {
    return YELLOW;
  }
  if (r_on && !g_on && !b_on) {
    return RED;
  }
  if (!r_on && g_on && !b_on) {
    return GREEN;
  }
  if (!r_on && !g_on && b_on) {
    return BLUE;
  }
  // Handle "impure" colors (Cyan, Magenta)
  if (!r_on && g_on && b_on) {
    // Cyan (G+B) -> Closest is Green or Blue. Pick Green.
    return GREEN;
  }
  if (r_on && !g_on) {
    // Magenta (R+B) -> Closest is Red or Blue. Pick Red.
    return RED;
  }
  // Handle the remaining corners (White-ish, Black-ish)
  if (r_on) {
    // All high (but not gray) -> White
    return WHITE;
  }
  // !r_on && !g_on && !b_on
  // All low (but not gray) -> Black
  return BLACK;
}

void EPaperSpectraE6::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperSpectraE6::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);
  this->data(0x00);
}

void EPaperSpectraE6::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->command(0x12);
  this->data(0x00);
}

void EPaperSpectraE6::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->command(0x07);
  this->data(0xA5);
}

void EPaperSpectraE6::fill(Color color) {
  auto pixel_color = color_to_hex(color);

  // We store 2 pixels per byte
  this->buffer_.fill(pixel_color + (pixel_color << 4));
}

void EPaperSpectraE6::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperSpectraE6::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  auto pixel_bits = color_to_hex(color);
  uint32_t pixel_position = x + y * this->get_width_internal();
  uint32_t byte_position = pixel_position / 2;
  auto original = this->buffer_[byte_position];
  if ((pixel_position & 1) != 0) {
    this->buffer_[byte_position] = (original & 0xF0) | pixel_bits;
  } else {
    this->buffer_[byte_position] = (original & 0x0F) | (pixel_bits << 4);
  }
}

bool HOT EPaperSpectraE6::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0) {
    this->command(0x10);
  }

  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ != buffer_length) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->end_data_();
      ESP_LOGV(TAG, "Wrote %d bytes at %ums", buf_idx, (unsigned) millis());
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        return false;
      }
    }
  }
  // Finished the entire dataset
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->end_data_();
  }
  this->current_data_index_ = 0;
  return true;
}
}  // namespace esphome::epaper_spi
