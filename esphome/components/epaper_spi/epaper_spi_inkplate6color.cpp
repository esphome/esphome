// Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library (src/boards/Inkplate6COLOR)

#include "epaper_spi_inkplate6color.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.inkplate6color";
static constexpr unsigned char GRAY_THRESHOLD = 50;

// Native hardware color codes for this panel's 4-bit color values.
enum Inkplate6ColorHex : uint8_t {
  BLACK = 0,
  WHITE = 1,
  GREEN = 2,
  BLUE = 3,
  RED = 4,
  YELLOW = 5,
  ORANGE = 6,
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

  if (r_on && !b_on) {
    // Between red and yellow: unlike most other panels in this component, this one has a
    // dedicated orange ink, so split the red->yellow gradient in three instead of collapsing
    // it to two. Named orange (e.g. 0xFFA500) has g close to the midpoint, so the shared
    // g_on (>128) threshold used elsewhere can't tell it apart from yellow.
    if (color.g > 170) {
      return YELLOW;
    }
    if (color.g > 85) {
      return ORANGE;
    }
    return RED;
  }
  if (!r_on && g_on && !b_on) {
    return GREEN;
  }
  if (!r_on && !g_on && b_on) {
    return BLUE;
  }
  // Handle "impure" colors (cyan, magenta) by folding into the closest primary.
  if (!r_on && g_on && b_on) {
    return GREEN;  // cyan
  }
  if (r_on && !g_on) {
    return RED;  // magenta
  }
  if (r_on) {
    // All high (but not gray) -> white
    return WHITE;
  }
  // !r_on && !g_on && !b_on
  // All low (but not gray) -> black
  return BLACK;
}

void EPaperInkplate6Color::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperInkplate6Color::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);
}

void EPaperInkplate6Color::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");  // full refresh only; partial is unused
  this->cmd_data(0x12, {0x00});
}

void EPaperInkplate6Color::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

void EPaperInkplate6Color::fill(Color color) {
  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  auto pixel_color = color_to_hex(color);

  // We store 2 pixels per byte
  this->buffer_.fill(pixel_color + (pixel_color << 4));
}

void EPaperInkplate6Color::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperInkplate6Color::draw_pixel_at(int x, int y, Color color) {
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

bool HOT EPaperInkplate6Color::transfer_data() {
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
      this->disable();
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
    this->disable();
  }
  this->current_data_index_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
