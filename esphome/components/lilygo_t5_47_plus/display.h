#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

#include "epd_driver.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace lilygo_t5_47_plus {

class LilygoT547PlusDisplay : public display::DisplayBuffer {
 public:
  float get_setup_priority() const override;

  void dump_config() override;

  void display();
  void update() override;
  void fill(Color color) override;

  void setup() override;
  void on_safe_shutdown() override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_GRAYSCALE; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_width_internal() override { return 960; }

  int get_height_internal() override { return 540; }

  size_t get_buffer_length_();
};

}  // namespace lilygo_t5_47_plus
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
