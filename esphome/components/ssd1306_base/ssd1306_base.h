#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace ssd1306_base {

enum SSD1306Model {
  SSD1306_MODEL_128_32 = 0,
  SSD1306_MODEL_128_64,
  SSD1306_MODEL_96_16,
  SSD1306_MODEL_64_48,
  SSD1306_MODEL_64_32,
  SSD1306_MODEL_72_40,
  SH1106_MODEL_128_32,
  SH1106_MODEL_128_64,
  SH1106_MODEL_96_16,
  SH1106_MODEL_64_48,
  SH1107_MODEL_128_64,
  SH1107_MODEL_128_128,
  SSD1305_MODEL_128_32,
  SSD1305_MODEL_128_64,
};

class SSD1306 : public display::DisplayBuffer {
 public:
  void setup() override;

  void display();

  void update() override;

  void set_model(SSD1306Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_external_vcc(bool external_vcc) { this->external_vcc_ = external_vcc; }
  void init_contrast(float contrast) { this->contrast_ = contrast; }
  float get_contrast();
  void set_contrast(float contrast);
  float get_brightness();
  void init_brightness(float brightness) { this->brightness_ = brightness; }
  void set_brightness(float brightness);
  void init_flip_x(bool flip_x) { this->flip_x_ = flip_x; }
  void init_flip_y(bool flip_y) { this->flip_y_ = flip_y; }
  void init_offset_x(uint8_t offset_x) { this->offset_x_ = offset_x; }
  void init_offset_y(uint8_t offset_y) { this->offset_y_ = offset_y; }
  void init_invert(bool invert) { this->invert_ = invert; }
  void set_invert(bool invert);
  bool is_on();
  void turn_on();
  void turn_off();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void write_display_data() = 0;
  void init_reset_();

  bool is_sh1106_() const;
  bool is_sh1107_() const;
  bool is_ssd1305_() const;

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  const char *model_str_();

  SSD1306Model model_{SSD1306_MODEL_128_64};
  GPIOPin *reset_pin_{nullptr};
  bool external_vcc_{false};
  bool is_on_{false};
  float contrast_{1.0};
  float brightness_{1.0};
  bool flip_x_{true};
  bool flip_y_{true};
  uint8_t offset_x_{0};
  uint8_t offset_y_{0};
  bool invert_{false};
};

}  // namespace ssd1306_base
}  // namespace esphome
