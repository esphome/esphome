#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace ssd1306_base {

enum SSD1306Model {
  SSD1306_MODEL_128_32 = 0,
  SSD1306_MODEL_128_64,
  SSD1306_MODEL_96_16,
  SSD1306_MODEL_64_48,
  SH1106_MODEL_128_32,
  SH1106_MODEL_128_64,
  SH1106_MODEL_96_16,
  SH1106_MODEL_64_48,
};

class SSD1306 : public PollingComponent, public display::DisplayBuffer {
 public:
  void setup() override;

  void display();

  void update() override;

  void set_model(SSD1306Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_external_vcc(bool external_vcc) { this->external_vcc_ = external_vcc; }
  void set_brightness(float brightness) { this->brightness_ = brightness; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(int color) override;

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void write_display_data() = 0;
  void init_reset_();

  bool is_sh1106_() const;

  void draw_absolute_pixel_internal(int x, int y, int color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  const char *model_str_();

  SSD1306Model model_{SSD1306_MODEL_128_64};
  GPIOPin *reset_pin_{nullptr};
  bool external_vcc_{false};
  float brightness_{1.0};
};

}  // namespace ssd1306_base
}  // namespace esphome
