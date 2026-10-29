#include "spi_led_strip.h"
#include "esphome/core/helpers.h"

namespace esphome::spi_led_strip {

SpiLedStrip::SpiLedStrip(uint16_t num_leds) {
  this->num_leds_ = num_leds;
  RAMAllocator<uint8_t> allocator;
  this->buffer_size_ = num_leds * 4 + 8;
  this->internal_accessor_.color_data = allocator.allocate(this->buffer_size_);
  if (this->internal_accessor_.color_data == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", this->buffer_size_);
    return;
  }

  this->internal_accessor_.effect_data = allocator.allocate(num_leds);
  if (this->internal_accessor_.effect_data == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", num_leds);
    return;
  }
  memset(this->internal_accessor_.color_data, 0xFF, this->buffer_size_);
  memset(this->internal_accessor_.color_data, 0, 4);
  this->accessor_ = &this->internal_accessor_;
}
void SpiLedStrip::setup() {
  if (this->internal_accessor_.effect_data == nullptr || this->internal_accessor_.color_data == nullptr) {
    this->mark_failed();
    return;
  }
  this->spi_setup();
}
light::LightTraits SpiLedStrip::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}
void SpiLedStrip::dump_config() {
  esph_log_config(TAG,
                  "SPI LED Strip:\n"
                  "  LEDs: %d",
                  this->num_leds_);
  if (this->data_rate_ >= spi::DATA_RATE_1MHZ) {
    esph_log_config(TAG, "  Data rate: %uMHz", (unsigned) (this->data_rate_ / 1000000));
  } else {
    esph_log_config(TAG, "  Data rate: %ukHz", (unsigned) (this->data_rate_ / 1000));
  }
}
void SpiLedStrip::write_state(light::LightState *state) {
  if (this->is_failed())
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  {
    char strbuf[49];  // format_hex_pretty_size(16) = 48, fits 16 bytes
    size_t len = std::min(this->buffer_size_, (size_t) 16);
    format_hex_pretty_to(strbuf, sizeof(strbuf), this->internal_accessor_.color_data, len, ' ');
    esph_log_v(TAG, "write_state: buf = %s", strbuf);
  }
#endif
  this->enable();
  this->write_array(this->internal_accessor_.color_data, this->buffer_size_);
  this->disable();
}
}  // namespace esphome::spi_led_strip
