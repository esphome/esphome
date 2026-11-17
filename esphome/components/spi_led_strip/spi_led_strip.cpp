#include "spi_led_strip.h"
#include "esphome/core/helpers.h"

namespace esphome::spi_led_strip {

constexpr size_t COLOR_DATA_OFFSET = 5;

SpiLedStrip::SpiLedStrip(uint16_t num_leds) : num_leds_(num_leds) {}
void SpiLedStrip::set_buffer(uint8_t *buffer) {
  if (buffer == nullptr)
    return;
  this->internal_accessor_.color_data = buffer + COLOR_DATA_OFFSET;
  this->internal_accessor_.effect_data = buffer + frame_size(this->num_leds_);
}
void SpiLedStrip::setup() {
  if (!this->setup_check(this->internal_accessor_.color_data != nullptr, LOG_STR(ESP_LOG_MSG_ALLOC_FAIL)))
    return;
  uint8_t *buf = this->internal_accessor_.color_data - COLOR_DATA_OFFSET;
  memset(buf, 0xFF, frame_size(this->num_leds_));
  memset(buf, 0, 4);
  this->accessor_ = &this->internal_accessor_;

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
  uint8_t *buffer = this->internal_accessor_.color_data - COLOR_DATA_OFFSET;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  {
    char strbuf[49];  // format_hex_pretty_size(16) = 48, fits 16 bytes
    size_t len = std::min(frame_size(this->num_leds_), (size_t) 16);
    format_hex_pretty_to(strbuf, sizeof(strbuf), buffer, len, ' ');
    esph_log_v(TAG, "write_state: buf = %s", strbuf);
  }
#endif
  this->enable();
  this->write_array(buffer, frame_size(this->num_leds_));
  this->disable();
}
}  // namespace esphome::spi_led_strip
