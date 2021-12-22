#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace ssd1351_base {

enum SSD1351Model {
  SSD1351_MODEL_128_96 = 0,
  SSD1351_MODEL_128_128,
};

class SSD1351 : public PollingComponent, public display::DisplayBuffer {
 public:
  void setup() override;

  void display();

  void update() override;

  void set_model(SSD1351Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void init_brightness(float brightness) { this->brightness_ = brightness; }
  void set_brightness(float brightness);
  bool is_on();
  void turn_on();
  void turn_off();

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(Color color) override;

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void data(uint8_t value) = 0;
  virtual void write_display_data() = 0;
  void init_reset_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  const char *model_str_();

  SSD1351Model model_{SSD1351_MODEL_128_96};
  GPIOPin *reset_pin_{nullptr};
  bool is_on_{false};
  float brightness_{1.0};
};

}  // namespace ssd1351_base
}  // namespace esphome
