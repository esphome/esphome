#pragma once

#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

enum DisplayMode { MODE_PARTIAL = 0, MODE_FULL, MODE_FAST, MODE_GRAYSCALE4 };

class WaveshareEPaper4P2InV2 : public WaveshareEPaper {
 public:
  void dump_config() override;
  void display() override;
  void initialize() override;
  void deep_sleep() override;

  void set_full_update_every(uint32_t full_update_every);
  void set_display_mode(DisplayMode mode) { this->display_mode_ = mode; }

 protected:
  void initialize_internal_(DisplayMode mode);
  void update_(DisplayMode mode);
  void turn_on_display_(DisplayMode mode);

  void write_lut_();

  void reset_();
  void set_window_(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2);
  void set_cursor_(uint16_t x, uint16_t y);
  void clear_();

  int get_width_internal() override;
  int get_height_internal() override;
  uint32_t get_buffer_length_() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t idle_timeout_() override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  bool is_busy_{false};
  DisplayMode display_mode_{MODE_FAST};
};

}  // namespace waveshare_epaper
}  // namespace esphome
