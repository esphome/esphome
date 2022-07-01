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

using Mapping = struct {
  FastLEDBus *const bus_;
  const uint16_t num_chips_;
  const uint16_t ofs_;
  const uint8_t channel_offset_;
  const uint16_t repeat_distance_;
};

class Output : public esphome::output::FloatOutput, public Component {
 protected:
  const uint8_t len_;
  Mapping *const mappings_;

 public:
  // repeated_distance must be big to stop the set loop
  Output(uint8_t len, Mapping *const mappings) : len_(len), mappings_{mappings} {}

  void setup() override;
  void dump_config() override {}

  void write_state(float state) override;
};

}  // namespace fastled_bus
}  // namespace esphome

#endif  // USE_ARDUINO
