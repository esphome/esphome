#include "spi_led_strip.h"
#include "esphome/core/helpers.h"

namespace esphome::spi_led_strip {

void SpiLedStrip::setup() {
  RAMAllocator<uint8_t> allocator;
  if (!this->buffer_.allocate_and_setup(&allocator)) {
    ESP_LOGE(TAG, "Cannot allocate color buffer");
    this->mark_failed();
    return;
  }
  memset(this->buffer_.get_led_data() + 4, 0xFF, this->buffer_.get_led_data_bytes() - 4);
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
                  this->buffer_.size());
  if (this->data_rate_ >= spi::DATA_RATE_1MHZ) {
    esph_log_config(TAG, "  Data rate: %uMHz", (unsigned) (this->data_rate_ / 1000000));
  } else {
    esph_log_config(TAG, "  Data rate: %ukHz", (unsigned) (this->data_rate_ / 1000));
  }
}
void SpiLedStrip::write_state(light::LightState *state) {
  if (!this->is_ready()) {
    return;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  {
    char strbuf[49];  // format_hex_pretty_size(16) = 48, fits 16 bytes
    size_t len = std::min(this->buffer_.get_led_data_bytes(), (size_t) 16);
    format_hex_pretty_to(strbuf, sizeof(strbuf), this->buffer_.get_led_data(), len, ' ');
    esph_log_v(TAG, "write_state: buf = %s", strbuf);
  }
#endif
  this->enable();
  this->write_array(this->buffer_.get_led_data(), this->buffer_.get_led_data_bytes());
  this->disable();
}
}  // namespace esphome::spi_led_strip
