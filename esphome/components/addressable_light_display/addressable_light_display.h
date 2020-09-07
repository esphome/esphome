#pragma once

#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace addressable_light_display {

class AddressableLightDisplay : public display::DisplayBuffer, public PollingComponent {
 public:
  light::AddressableLight *get_light() const { return this->light_; }

  void set_width(int32_t width) { width_ = width; }
  void set_height(int32_t height) { height_ = height; }
  void set_light(light::LightState* state) { light_ = static_cast<light::AddressableLight *>(state->get_output()); }
  void set_enabled(bool enabled) { enabled_ = enabled; }

  void setup() override;

 protected:
  int get_width_internal() override;
  int get_height_internal() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  virtual void update() override;
  void display();

  light::AddressableLight *light_;
  bool enabled_;
  int32_t width_;
  int32_t height_;
  std::vector<Color> buffer_;
};
}  // namespace addressable_light_display
}  // namespace esphome
