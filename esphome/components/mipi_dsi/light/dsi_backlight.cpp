#include "dsi_backlight.h"
namespace esphome {
namespace mipi_dsi {

static const char *const TAG = "light.mipi_dsi";

light::LightTraits DsiBacklight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}
void DsiBacklight::write_state(light::LightState *state) {
  float bright;
  state->current_values_as_brightness(&bright);
  auto brightness = (uint8_t) (bright * 255.0f);
  if (brightness < 3)
    brightness = 3;
  if (this->inverted_)
    brightness = 0xFF - brightness;
  this->brightness_ = brightness;
  if (!this->write_byte(this->pwm_register_, this->brightness_)) {
    ESP_LOGE(TAG, "Failed to write brightness to DSI backlight");
  }
};
}  // namespace mipi_dsi
}  // namespace esphome
