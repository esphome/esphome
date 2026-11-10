#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"
#include "driver/spi_master.h"
#include "encoder.h"

namespace esphome::brizlabs_led_string {

static constexpr int MAX_ZONES = 15;  // 4-bit zone field, 0 = all

// Two-wire LED string: color frames are pulse-coded power dips, shaped by SPI MOSI (encoder.h)
// One addressable "pixel" per zone, with each zone comprising one or more actual LED modules
class BrizLabsLEDString : public light::AddressableLight {
 public:
  BrizLabsLEDString(int pin, int num_zones) : pin_(pin), num_zones_(num_zones) {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  int32_t size() const override { return this->num_zones_; }
  light::LightTraits get_traits() override { return this->internal_accessor_.channel_colors.traits(); }
  void write_state(light::LightState *state) override;
  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(this->num_zones_); }

 protected:
  bool send_frame_(uint8_t zone, uint8_t r, uint8_t g, uint8_t b);  // zone 0 = whole string
  bool transmit_all_();

  int pin_;
  int num_zones_;
  spi_device_handle_t dev_{nullptr};
  alignas(4) uint8_t frame_[FRAME_BYTES]{};
  uint8_t buf_[MAX_ZONES * 3]{};
  uint8_t effect_data_[MAX_ZONES]{};
  light::CorrectedBufferColorAccessor internal_accessor_{this->buf_, this->effect_data_, {0, 1, 2}, &this->correction_};
};

}  // namespace esphome::brizlabs_led_string

#endif  // USE_ESP32
