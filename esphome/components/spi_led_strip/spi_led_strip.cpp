#include "spi_led_strip.h"

namespace esphome {
namespace spi_led_strip {

SpiLedStrip::SpiLedStrip(Protocol protocol, light::ChannelMap channel_map, uint16_t num_leds)
    : protocol_(protocol), channel_map_(channel_map), num_leds_(num_leds) {
  this->buffer_size_ = this->num_leds_ * ((this->protocol_ == DOTSTAR) ? 4 : this->channel_map_.get_channel_count()) +
                       ((this->protocol_ == DOTSTAR) ? 8 : 0);

  RAMAllocator<uint8_t> allocator;
  this->buf_ = allocator.allocate(this->buffer_size_);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", this->buffer_size_);
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", this->num_leds_);
    return;
  }

  this->base_ = this->buf_;
  this->address_multiplier_ = this->channel_map_.get_channel_count();

  switch (this->protocol_) {
    case Protocol::DOTSTAR: {
      memset(this->buf_, 0xFF, this->buffer_size_);
      memset(this->buf_, 0x00, 4);     // Start bytes
      this->base_ += 5;                // Skip brightness and start bytes
      this->address_multiplier_ += 1;  // Extra brightness byte
      break;
    }
    case Protocol::RAW: {
      memset(this->buf_, 0x00, this->buffer_size_);
      break;
    }
    default: {
      ESP_LOGE(TAG, "Unknown protocol %u", this->num_leds_);
      return;
    }
  }
}
void SpiLedStrip::setup() {
  if (this->buf_ == nullptr || this->effect_data_ == nullptr) {
    this->mark_failed();
    return;
  }
  this->spi_setup();
}
light::LightTraits SpiLedStrip::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({this->channel_map_.get_color_mode()});
  if (this->channel_map_.get_color_mode() == light::ColorMode::RGB_COLD_WARM_WHITE) {
    traits.set_min_mireds(1'000'000. / this->cold_white_color_temperature_);
    traits.set_max_mireds(1'000'000. / this->warm_white_color_temperature_);
  }
  return traits;
}
void SpiLedStrip::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI LED Strip:");
  ESP_LOGCONFIG(TAG, "  LEDs: %d", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Protocol: %s",
                this->protocol_ == DOTSTAR ? "DOTSTAR"
                : this->protocol_ == RAW   ? "RAW"
                                           : "Unknown");
  ESP_LOGCONFIG(TAG, "  Channel Map: %s (%u channels)", this->channel_map_.get_str(),
                this->channel_map_.get_channel_count());
  ESP_LOGCONFIG(TAG, "  Color mode: %s",
                (this->channel_map_.get_color_mode() == light::ColorMode::RGB_COLD_WARM_WHITE) ? "RGBCCT"
                : (this->channel_map_.get_color_mode() == light::ColorMode::RGB_WHITE)         ? "RGBW"
                                                                                               : "RGB");
  if (this->data_rate_ >= spi::DATA_RATE_1MHZ) {
    ESP_LOGCONFIG(TAG, "  Data rate: %uMHz", (unsigned) (this->data_rate_ / 1000000));
  } else {
    ESP_LOGCONFIG(TAG, "  Data rate: %ukHz", (unsigned) (this->data_rate_ / 1000));
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
    ESP_LOGV(TAG, "write_state: buf = %s", strbuf);
  }
  this->enable();
  this->write_array(this->buf_, this->buffer_size_);
  this->disable();
}
light::ESPColorView SpiLedStrip::get_view_internal(int32_t index) const {
  uint8_t *led_base = this->base_ + (index * this->address_multiplier_);

  uint8_t *r_ptr = this->channel_map_.get_address_by_channel_name(led_base, light::ChannelMap::ChannelName::R);
  uint8_t *g_ptr = this->channel_map_.get_address_by_channel_name(led_base, light::ChannelMap::ChannelName::G);
  uint8_t *b_ptr = this->channel_map_.get_address_by_channel_name(led_base, light::ChannelMap::ChannelName::B);
  uint8_t *w_ptr = this->channel_map_.get_address_by_channel_name(led_base, light::ChannelMap::ChannelName::W);

  return {r_ptr, g_ptr, b_ptr, w_ptr, &this->effect_data_[index], &this->correction_};
}
}  // namespace spi_led_strip
}  // namespace esphome
