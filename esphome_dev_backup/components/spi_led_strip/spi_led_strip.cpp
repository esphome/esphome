#include "spi_led_strip.h"

namespace esphome {
namespace spi_led_strip {

SpiLedStrip::SpiLedStrip(uint16_t num_leds) {
  this->num_leds_ = num_leds;
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_size_ = num_leds * 4 + 8;
  this->buf_ = allocator.allocate(this->buffer_size_);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", this->buffer_size_);
    return;
  }

  this->effect_data_ = allocator.allocate(num_leds);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", num_leds);
    return;
  }
  memset(this->buf_, 0xFF, this->buffer_size_);
  memset(this->buf_, 0, 4);
}
void SpiLedStrip::setup() {
  if (this->effect_data_ == nullptr || this->buf_ == nullptr) {
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
  esph_log_config(TAG, "SPI LED Strip:");
  esph_log_config(TAG, "  LEDs: %d", this->num_leds_);
  if (this->data_rate_ >= spi::DATA_RATE_1MHZ) {
    esph_log_config(TAG, "  Data rate: %uMHz", (unsigned) (this->data_rate_ / 1000000));
  } else {
    esph_log_config(TAG, "  Data rate: %ukHz", (unsigned) (this->data_rate_ / 1000));
  }
}
void SpiLedStrip::write_state(light::LightState *state) {
  if (this->is_failed())
    return;
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    char strbuf[49];
    size_t len = std::min(this->buffer_size_, (size_t) (sizeof(strbuf) - 1) / 3);
    memset(strbuf, 0, sizeof(strbuf));
    for (size_t i = 0; i != len; i++) {
      sprintf(strbuf + i * 3, "%02X ", this->buf_[i]);
    }
    esph_log_v(TAG, "write_state: buf = %s", strbuf);
  }
  this->enable();
  this->write_array(this->buf_, this->buffer_size_);
  this->disable();
}
light::ESPColorView SpiLedStrip::get_view_internal(int32_t index) const {
  size_t pos = index * 4 + 5;
  return {this->buf_ + pos + 2,       this->buf_ + pos + 1, this->buf_ + pos + 0, nullptr,
          this->effect_data_ + index, &this->correction_};
}
}  // namespace spi_led_strip
}  // namespace esphome
