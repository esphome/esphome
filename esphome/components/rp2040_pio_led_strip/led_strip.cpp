#include "led_strip.h"

#ifdef USE_RP2040

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico/stdlib.h>

namespace esphome {
namespace rp2040_pio_led_strip {

static const char *TAG = "rp2040_pio_led_strip";

void RP2040PIOLEDStripLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RP2040 LED Strip...");

  size_t buffer_size = this->get_buffer_size_();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", buffer_size);
    this->mark_failed();
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", this->num_leds_);
    this->mark_failed();
    return;
  }

  // Select PIO instance to use (0 or 1)
  this->pio_ = pio0;
  if (this->pio_ == nullptr) {
    ESP_LOGE(TAG, "Failed to claim PIO instance");
    this->mark_failed();
    return;
  }

  // Load the assembled program into the PIO and get its location in the PIO's instruction memory
  uint offset = pio_add_program(this->pio_, this->program_);

  // Configure the state machine's PIO, and start it
  this->sm_ = pio_claim_unused_sm(this->pio_, true);
  if (this->sm_ < 0) {
    ESP_LOGE(TAG, "Failed to claim PIO state machine");
    this->mark_failed();
    return;
  }
  this->init_(this->pio_, this->sm_, offset, this->pin_, this->max_refresh_rate_);
}

void RP2040PIOLEDStripLightOutput::write_state(light::LightState *state) {
  ESP_LOGVV(TAG, "Writing state...");

  if (this->is_failed()) {
    ESP_LOGW(TAG, "Light is in failed state, not writing state.");
    return;
  }

  if (this->buf_ == nullptr) {
    ESP_LOGW(TAG, "Buffer is null, not writing state.");
    return;
  }

  // assemble bits in buffer to 32 bit words with ex for GBR: 0bGGGGGGGGRRRRRRRRBBBBBBBB00000000
  for (int i = 0; i < this->num_leds_; i++) {
    uint8_t multiplier = this->is_rgbw_ ? 4 : 3;
    uint8_t c1 = this->buf_[(i * multiplier) + 0];
    uint8_t c2 = this->buf_[(i * multiplier) + 1];
    uint8_t c3 = this->buf_[(i * multiplier) + 2];
    uint8_t w = this->is_rgbw_ ? this->buf_[(i * 4) + 3] : 0;
    uint32_t color = encode_uint32(c1, c2, c3, w);
    pio_sm_put_blocking(this->pio_, this->sm_, color);
  }
}

light::ESPColorView RP2040PIOLEDStripLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0, g = 0, b = 0, w = 0;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      r = 0;
      g = 1;
      b = 2;
      break;
    case ORDER_RBG:
      r = 0;
      g = 2;
      b = 1;
      break;
    case ORDER_GRB:
      r = 1;
      g = 0;
      b = 2;
      break;
    case ORDER_GBR:
      r = 2;
      g = 0;
      b = 1;
      break;
    case ORDER_BGR:
      r = 2;
      g = 1;
      b = 0;
      break;
    case ORDER_BRG:
      r = 1;
      g = 2;
      b = 0;
      break;
  }
  uint8_t multiplier = this->is_rgbw_ ? 4 : 3;
  return {this->buf_ + (index * multiplier) + r,
          this->buf_ + (index * multiplier) + g,
          this->buf_ + (index * multiplier) + b,
          this->is_rgbw_ ? this->buf_ + (index * multiplier) + 3 : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void RP2040PIOLEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PIO LED Strip Light Output:");
  ESP_LOGCONFIG(TAG, "  Pin: GPIO%d", this->pin_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %d", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  RGBW: %s", YESNO(this->is_rgbw_));
  ESP_LOGCONFIG(TAG, "  RGB Order: %s", rgb_order_to_string(this->rgb_order_));
  ESP_LOGCONFIG(TAG, "  Max Refresh Rate: %f Hz", this->max_refresh_rate_);
}

float RP2040PIOLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace rp2040_pio_led_strip
}  // namespace esphome

#endif
