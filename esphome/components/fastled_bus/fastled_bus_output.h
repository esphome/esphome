#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP32_RAW_PIN_ORDER
#define FASTLED_RMT_BUILTIN_DRIVER true

// Avoid annoying compiler messages
#define FASTLED_INTERNAL

#include "fastled_bus.h"

namespace esphome {
namespace fastled_bus {

class Output : public esphome::output::FloatOutput, public Component {
 protected:
  const uint16_t num_chips_;
  const uint16_t ofs_;
  const uint8_t channel_offset_;
  uint16_t repeat_distance_{0};
  FastLEDBus *bus_{nullptr};

 public:
  // repeated_distance must be big to stop the set loop
  Output(uint16_t num_chips, uint16_t ofs, uint8_t channel_offset)
      : num_chips_(num_chips), ofs_(ofs), channel_offset_(channel_offset) {}

  void set_bus(FastLEDBus *bus) {
    this->bus_ = bus;
    this->repeat_distance_ = this->bus_->chip_channels_ * this->bus_->num_chips_;
  }
  void set_repeat_distance(uint16_t repeat_distance) { this->repeat_distance_ = repeat_distance; }

  void setup() override;
  void dump_config() override {}

  void write_state(float state) override;
};

}  // namespace fastled_bus
}  // namespace esphome

#endif  // USE_ARDUINO
