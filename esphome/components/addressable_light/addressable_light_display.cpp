#include "addressable_light_display.h"
#include "esphome/core/log.h"

namespace esphome::addressable_light {

ESPHOME_LOG_TAG(TAG, "addressable_light.display");

int AddressableLightDisplay::get_width_internal() { return this->width_; }
int AddressableLightDisplay::get_height_internal() { return this->height_; }

void AddressableLightDisplay::update() {
  if (!this->enabled_)
    return;

  this->do_update_();
  this->display();
}

void AddressableLightDisplay::display() {
  bool dirty = false;
  Color *c;

  for (int32_t offset = 0; offset < this->width_ * this->height_; offset++) {
    c = &(this->addressable_light_buffer_[offset]);

    light::ESPColorView pixel = (*this->light_)[offset];

    // Track the original values for the pixel view. If it has changed updating, then
    // we trigger a redraw. Avoiding redraws == avoiding flicker!
    const Color old_raw_color = pixel.get_raw();
    pixel.set(Color{c->r, c->g, c->b, c->w});

    // If the actual value of the pixel changed, then schedule a redraw.
    if (pixel.get_raw() != old_raw_color) {
      dirty = true;
    }
  }

  if (dirty) {
    this->light_->schedule_show();
  }
}

void HOT AddressableLightDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  if (this->pixel_mapper_f_.has_value()) {
    // Params are passed by reference, so they may be modified in call.
    int index = (*this->pixel_mapper_f_)(x, y);
    if (index < 0 || index >= this->width_ * this->height_)
      return;
    this->addressable_light_buffer_[index] = color;
  } else {
    this->addressable_light_buffer_[y * this->get_width_internal() + x] = color;
  }
}
}  // namespace esphome::addressable_light
