#include "neokey.h"

namespace esphome {
namespace neokey {

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