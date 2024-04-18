#include "neokey.h"

namespace esphome {
namespace neokey {

#ifdef USE_BINARY_SENSOR
void NeoKeyBinarySensor::keys_update(uint8_t keys) {
  bool pressed = keys & (1 << key_);
  if (pressed != this->state)
    this->publish_state(pressed);
}
#endif

#ifdef USE_LIGHT
void NeoKeyLight::setup() {
  // Byte each for Red, Green, and Blue
  this->buf_ = new uint8_t[this->size() * NUM_BYTES_PER_LED];
  this->effect_data_ = new uint8_t[this->size()];

  // Clear buffer
  memset(this->buf_, 0x00, this->size() * NUM_BYTES_PER_LED);
  memset(this->effect_data_, 0x00, this->size());
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
#endif

void NeoKeyComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NeoKey...");

  if (!this->hub_.begin(this->address_)) {
    this->mark_failed();
    return;
  }

  // Pulse all the LEDs on to show we're working
  for (size_t i = 0; i < this->hub_.pixels.numPixels(); i++) {
    this->hub_.pixels.setPixelColor(i, 0x808080);  // make each LED white
    this->hub_.pixels.show();
    delay(50);
  }

  for (size_t i = 0; i < this->hub_.pixels.numPixels(); i++) {
    this->hub_.pixels.setPixelColor(i, 0x000000);
    this->hub_.pixels.show();
    delay(50);
  }
}

void NeoKeyComponent::update() {
  uint8_t keys = this->hub_.read();
  ESP_LOGVV(TAG, "Keys: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(keys));

  if (this->listeners_.empty())
    return;

  for (auto &listener : this->listeners_)
    listener->keys_update(keys);
}

void NeoKeyComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "NeoKey:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with NeoKey failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace neokey
}  // namespace esphome