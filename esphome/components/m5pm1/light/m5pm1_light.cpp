#include "m5pm1_light.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::m5pm1 {

static const char *TAG = "m5pm1.light";

void M5PM1AddressableLight::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "NeoPixel GPIO pin is not configured");
    this->mark_failed();
    return;
  }
  // first configure GPIO0 as a normal output.
  this->pin_->setup();

  // then switch GPIO0 to the M5PM1 NeoPixel alternate function.
  if (!this->parent_->set_pin_function(this->pin_->get_pin(), M5PM1PinFunction::M5PM1_PIN_FUNCTION_ALTERNATE)) {
    ESP_LOGE(TAG, "Failed to configure NeoPixel GPIO function");
    this->mark_failed();
    return;
  }

  RAMAllocator<uint8_t> allocator;

  // Allocate memory for addressable light buffer
  // with the size of (number_of_led * RGB channel)
  this->buf_ = allocator.allocate(this->num_leds_ * 3);

  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer.");
    this->mark_failed();
    return;
  }

  memset(this->buf_, 0x00, this->num_leds_ * 3);

  this->effect_data_ = allocator.allocate(this->num_leds_);

  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data.");
    this->mark_failed();
    return;
  }

  memset(this->effect_data_, 0x00, this->num_leds_);

  // Enable LED, refresh RAM datazone
  this->parent_->set_neopixel_count(this->num_leds_);
}

void M5PM1AddressableLight::write_state(light::LightState *state) {
  uint8_t hwbuf[this->num_leds_ * 2];

  uint8_t *src = this->buf_;
  uint8_t *dst = hwbuf;

  for (int i = 0; i < num_leds_; i++) {
    uint8_t r = *src++;
    uint8_t g = *src++;
    uint8_t b = *src++;

    uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

    *dst++ = rgb565 & 0xFF;
    *dst++ = (rgb565 >> 8) & 0xFF;
  }

  if (!this->parent_->write_neopixels(reinterpret_cast<const uint16_t *>(hwbuf), this->num_leds_)) {
    this->status_set_warning("Error when writing LED RAM data");
    return;
  }

  delay(5);

  // trigger the refresh
  if (!this->parent_->refresh_neopixels()) {
    this->status_set_warning("Failed to refresh NeoPixel data");
  }
}

light::ESPColorView M5PM1AddressableLight::get_view_internal(int32_t index) const {
  size_t pos = index * 3;

  return {
      this->buf_ + pos, this->buf_ + pos + 1,       this->buf_ + pos + 2,
      nullptr,          this->effect_data_ + index, &this->correction_,
  };
}

void M5PM1AddressableLight::clear_effect_data() { memset(this->effect_data_, 0x00, this->num_leds_); }

}  // namespace esphome::m5pm1
