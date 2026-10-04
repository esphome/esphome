#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace sh1122_base {

enum SH1122Model {
  SH1122_MODEL_256_64 = 0,
};

class SH1122 : public display::DisplayBuffer {
 public:
  void setup() override;

  void display();

  void update() override;

  void set_model(SH1122Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void init_brightness(float brightness) { this->brightness_ = brightness; }
  void set_brightness(float brightness);
  bool is_on();
  void turn_on();
  void turn_off();

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_GRAYSCALE; }

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void command2(uint8_t value, uint8_t data) = 0;
  virtual void data(uint8_t value) = 0;
  virtual void write_display_data() = 0;
  void init_reset_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  const char *model_str_();

  SH1122Model model_{SH1122_MODEL_256_64};
  GPIOPin *reset_pin_{nullptr};
  bool is_on_{false};
  float brightness_{1.0};
};

}  // namespace sh1122_base
}  // namespace esphome
