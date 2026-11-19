#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/light/addressable_light.h"
#include "driver/spi_master.h"
#include "encoder.h"

namespace esphome::brizlabs_led_string {

// Set from num_zones by light.py
static constexpr int NUM_ZONES = BRIZLABS_LED_STRING_ZONE_COUNT;

// Two-wire LED string: color frames are pulse-coded power dips, shaped by SPI MOSI (encoder.h)
// One addressable "pixel" per zone, with each zone comprising one or more actual LED modules
class BrizLabsLEDString final : public light::AddressableLight {
 public:
  explicit BrizLabsLEDString(int pin) : pin_(pin) {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  int32_t size() const override { return NUM_ZONES; }
  light::LightTraits get_traits() override { return this->internal_accessor_.channel_colors.traits(); }
  void write_state(light::LightState *state) override;
  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(NUM_ZONES); }

 protected:
  bool queue_frame_(uint8_t zone, uint8_t r, uint8_t g, uint8_t b);  // zone 0 = whole string
  bool transmit_all_();

  int pin_;
  spi_device_handle_t dev_{nullptr};
  // A queued frame and its transaction belong to the SPI driver until write_state() collects the result
  spi_transaction_t trans_[NUM_ZONES]{};
  uint8_t frames_[NUM_ZONES][FRAME_BYTES]{};
  uint8_t in_flight_{0};
  uint8_t buf_[NUM_ZONES * 3]{};
  uint8_t effect_data_[NUM_ZONES]{};
  light::CorrectedBufferColorAccessor internal_accessor_{this->buf_, this->effect_data_, {0, 1, 2}, &this->correction_};
};

}  // namespace esphome::brizlabs_led_string

#endif  // USE_ESP32
