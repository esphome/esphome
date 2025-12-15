#include "spi_led_strip.h"

namespace esphome {
namespace spi_led_strip {

void SpiLedStrip::setup() {
  RAMAllocator<uint8_t> allocator;
  this->buf_ = allocator.allocate(this->get_buffer_size());
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", this->get_buffer_size());
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", this->num_leds_);
    return;
  }
  memset(this->buf_, 0xFF, this->get_buffer_size());  // Initialize with 0xFF to be compatible with DotStar
  memset(this->buf_, 0x00, 4);  // Initialize first four bytes with 0x00 to be compatible with DotStar

  if (this->effect_data_ == nullptr || this->buf_ == nullptr) {
    this->mark_failed();
    return;
  }
  this->spi_setup();
}
light::LightTraits SpiLedStrip::get_traits() {
  auto traits = light::LightTraits();
  if (this->channel_map_.is_rgbcct()) {
    traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    traits.set_min_mireds(this->min_mireds_);
    traits.set_max_mireds(this->max_mireds_);
  } else if (this->channel_map_.is_rgbw()) {
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
  } else {
    traits.set_supported_color_modes({light::ColorMode::RGB});
  }
  return traits;
}
void SpiLedStrip::dump_config() {
  esph_log_config(TAG, "SPI LED Strip:");
  esph_log_config(TAG, "  LEDs: %d", this->num_leds_);
  esph_log_config(TAG, "  Protocol: %s",
                  this->protocol_ == DOTSTAR   ? "DOTSTAR"
                  : this->protocol_ == CLASSIC ? "CLASSIC"
                                               : "Unknown");
  esph_log_config(TAG, "  Channel Map: %s", this->channel_map_.to_string().c_str());
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
    size_t len = std::min(this->get_buffer_size(), (size_t) (sizeof(strbuf) - 1) / 3);
    memset(strbuf, 0, sizeof(strbuf));
    for (size_t i = 0; i != len; i++) {
      sprintf(strbuf + i * 3, "%02X ", this->buf_[i]);
    }
    esph_log_v(TAG, "write_state: buf = %s", strbuf);
  }
  this->enable();
  this->write_array(this->buf_, this->get_buffer_size());
  this->disable();
}
light::ESPColorView SpiLedStrip::get_view_internal(int32_t index) const {
  uint8_t multiplier = this->channel_map_.get_channel_count();
  if (this->protocol_ == DOTSTAR) {
    multiplier += 1;  // extra byte for brightness
  }
  uint8_t *base = this->buf_ + (index * multiplier);
  if (this->protocol_ == DOTSTAR) {
    base += 5;  // skip brightness and start bytes
  }

  uint8_t *r_ptr = this->channel_map_.get_pointer_position(base, "R");
  uint8_t *g_ptr = this->channel_map_.get_pointer_position(base, "G");
  uint8_t *b_ptr = this->channel_map_.get_pointer_position(base, "B");
  uint8_t *w_ptr = this->channel_map_.get_pointer_position(base, "W");

  return {r_ptr, g_ptr, b_ptr, w_ptr, &this->effect_data_[index], &this->correction_};
}
}  // namespace spi_led_strip
}  // namespace esphome
