#include "seesaw_neopixel.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.neopixel");

void SeesawNeopixel::setup() {
  if (!seesaw_hub_failed(this, this->parent_))
    this->parent_->setup_neopixel(this->pin_, this->num_leds_);
}

void SeesawNeopixel::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Seesaw NeoPixel:\n"
                "  Pin: %u\n"
                "  LEDs: %u",
                this->pin_, this->num_leds_);
}

void SeesawNeopixel::clear_effect_data() { memset(this->buf_ + 3 * this->num_leds_, 0, this->num_leds_); }

light::LightTraits SeesawNeopixel::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void SeesawNeopixel::write_state(light::LightState *state) {
  if (this->is_failed())
    return;
  this->mark_shown_();
  this->parent_->show_neopixel(this->buf_, 3 * this->num_leds_);
}

}  // namespace esphome::seesaw
