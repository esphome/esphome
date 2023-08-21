#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace spi_rgb_led {

#ifndef SPIRGBLED_DATA_RATE
#define SPIRGBLED_DATA_RATE spi::DATA_RATE_1MHZ
#endif  // SPIRGBLED_DATA_RATE

static const char *const TAG = "spi_rgb_led";
class SpiRgbLed : public light::AddressableLight,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                     SPIRBGLED_DATA_RATE> {
 public:
  void setup() {}

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void set_num_leds(uint16_t num_leds) {
    this->num_leds_ = num_leds;
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->buffer_size_ = num_leds * 4 + 8;
    this->buf_ = allocator.allocate(this->buffer_size_);
    if (this->buf_ == nullptr) {
      esph_log_e(TAG, "Failed to allocate buffer of size %u", this->buffer_size_);
      this->mark_failed();
      return;
    }

    this->effect_data_ = allocator.allocate(num_leds);
    if (this->effect_data_ == nullptr) {
      esph_log_e(TAG, "Failed to allocate effect data of size %u", num_leds);
      this->mark_failed();
      return;
    }
    memset(this->buf_, 0xFF, this->buffer_size_);
    memset(this->buf_, 0, 4);
  }

  void dump_config() {
    esph_log_config(TAG, "spi_rgb_led:");
    esph_log_config(TAG, "NumLeds: %d", this->num_leds_);
  }

  void write_state(light::LightState *state) override {
    this->enable();
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
    this->write_array(this->buf_, this->buffer_size_);
  }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    size_t pos = index * 4 + 5;
    return {this->buf_ + pos + 2,       this->buf_ + pos + 1, this->buf_ + pos + 0, nullptr,
            this->effect_data_ + index, &this->correction_};
  }

  size_t buffer_size_{};
  uint8_t *effect_data_{nullptr};
  uint8_t *buf_{nullptr};
  uint16_t num_leds_;
};

}  // namespace spi_rgb_led
}  // namespace esphome
