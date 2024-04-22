#include "neokey_light.h"

namespace esphome {
namespace neokey {

void NeoKeyLight::setup() {
  // Byte each for Red, Green, and Blue
  this->buf_ = new uint8_t[this->size() * NUM_BYTES_PER_LED];
  this->effect_data_ = new uint8_t[this->size()];

  // Clear buffer
  memset(this->buf_, 0x00, this->size() * NUM_BYTES_PER_LED);
  memset(this->effect_data_, 0x00, this->size());
}

void NeoKeyLight::set_neokey(NeoKeyComponent *neokey) { neokey_ = neokey; }

int32_t NeoKeyLight::size() { return this->neokey_ == nullptr ? 0 : this->neokey_->hub_.pixels.numPixels(); }

light::LightTraits NeoKeyLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void NeoKeyLight::write_state(light::LightState *state) {
  if (this->neokey_ == nullptr) {
    ESP_LOGW(TAG, "NeoKey component not initialized.");
    return;
  }

  ESP_LOGD(TAG, "Writing state...");
  for (size_t i = 0; i < this->size(); i++) {
    size_t pos = i * NUM_BYTES_PER_LED;
    uint8_t red = *(this->buf_ + pos + 0);
    uint8_t green = *(this->buf_ + pos + 1);
    uint8_t blue = *(this->buf_ + pos + 2);
    ESP_LOGD(TAG, "Red:   0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(red));
    ESP_LOGD(TAG, "Green: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(green));
    ESP_LOGD(TAG, "Blue:  0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(blue));

    this->neokey_->hub_.pixels.setPixelColor(i, red, green, blue);
  }
  this->neokey_->hub_.pixels.show();
}

void NeoKeyLight::clear_effect_data() {
  for (int i = 0; i < this->size(); i++)
    this->effect_data_[i] = 0;
}

light::ESPColorView NeoKeyLight::get_view_internal(int32_t index) {
  uint8_t *base = this->buf_ + (NUM_BYTES_PER_LED * index);

  return light::ESPColorView(base + 0, base + 1, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
}

}  // namespace neokey
}  // namespace esphome