#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/hal.h"
#include "esphome/core/version.h"

#include "epd_display.h"
#include "epd_highlevel.h"

namespace esphome::epdiy {

class EPDiyDisplay : public display::Display {
 public:
  float get_setup_priority() const override;
  void setup() override;
  void update() override;
  void on_shutdown() override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_GRAYSCALE; }

  int get_width_internal() override { return this->display_t_->width; };
  int get_height_internal() override { return this->display_t_->height; };

  size_t get_buffer_length() const { return this->display_t_->width / 2 * this->display_t_->height; }

  void set_power_off_delay_enabled(bool power_off_delay_enabled) {
    this->power_off_delay_enabled_ = power_off_delay_enabled;
  }

  void set_model_details(const EpdBoardDefinition *board_definition, const EpdDisplay_t *display_t,
                         enum EpdInitOptions init_options, uint16_t vcom) {
    this->board_definition_ = board_definition;
    this->display_t_ = display_t;
    this->init_options_ = init_options;
    this->vcom_mv_ = vcom;
  }

  void fill(Color color) override;

  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  void flush_screen_changes_();
  EpdiyHighlevelState state_;

  uint8_t *framebuffer_;

  const EpdBoardDefinition *board_definition_;
  const EpdDisplay_t *display_t_;
  enum EpdInitOptions init_options_;
  uint16_t vcom_mv_;

  bool power_off_delay_enabled_;
};

}  // namespace esphome::epdiy

#endif  // USE_ESP32
