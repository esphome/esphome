#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

// Soldered Inkplate 2: 104x212 black/white/red (BWR) e-paper, UC8xxx-family controller.
class EPaperInkplate2 final : public EPaperBase {
 public:
  EPaperInkplate2(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    // Dual-plane buffer: black/white plane followed by red plane, 1 bit per pixel each.
    this->buffer_length_ = this->row_width_ * this->height_ * 2;
  }

  void fill(Color color) override;
  void clear() override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  bool transfer_data() override;

  // Streams buffer_[current_data_index_ .. end) in chunks; returns false if it yields on MAX_TRANSFER_TIME.
  bool send_buffer_range_(size_t end, uint32_t start_time);
};

}  // namespace esphome::epaper_spi
