#pragma once

#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/light/addressable_light.h"

#include <vector>

namespace esphome {
namespace addressable_light {

class AddressableLightDisplay : public display::DisplayBuffer {
 public:
  light::AddressableLight *get_light() const { return this->light_; }

  void set_width(int32_t width) { width_ = width; }
  void set_height(int32_t height) { height_ = height; }
  void set_light(light::LightState *state) {
    light_state_ = state;
    light_ = static_cast<light::AddressableLight *>(state->get_output());
  }
  void set_enabled(bool enabled) {
    if (light_state_) {
      if (enabled_ && !enabled) {  // enabled -> disabled
        // - Tell the parent light to refresh, effectively wiping the display. Also
        //   restores the previous effect (if any).
        if (this->last_effect_index_.has_value()) {
          light_state_->make_call().set_effect(*this->last_effect_index_).perform();
        }

      } else if (!enabled_ && enabled) {  // disabled -> enabled
        // - Save the current effect index.
        this->last_effect_index_ = light_state_->get_current_effect_index();
        // - Disable any current effect.
        light_state_->make_call().set_effect(0).perform();
      }
    }
    enabled_ = enabled;
  }
  bool get_enabled() { return enabled_; }

  void set_pixel_mapper(std::function<int(int, int)> &&pixel_mapper_f) { this->pixel_mapper_f_ = pixel_mapper_f; }
  void setup() override;
  void display();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  int get_width_internal() override;
  int get_height_internal() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void update() override;

  light::LightState *light_state_;
  light::AddressableLight *light_;
  bool enabled_{true};
  int32_t width_;
  int32_t height_;
  std::vector<Color> addressable_light_buffer_;
  optional<uint32_t> last_effect_index_;
  optional<std::function<int(int, int)>> pixel_mapper_f_;
};
}  // namespace addressable_light
}  // namespace esphome
