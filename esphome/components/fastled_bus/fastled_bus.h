#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP32_RAW_PIN_ORDER
#define FASTLED_RMT_BUILTIN_DRIVER true

// Avoid annoying compiler messages
#define FASTLED_INTERNAL
#include "FastLED.h"

namespace esphome {
namespace fastled_bus {

class FastLEDBus : public Component {
 public:
  FastLEDBus(uint8_t chip_channels, uint16_t num_chips)
      : tag_("FastLEDBus"),
        chips_(new uint8_t[num_chips * chip_channels]),
        effect_data_(new uint8_t[num_chips]),
        chip_channels_(chip_channels),
        num_chips_(num_chips) {}

 protected:
  CLEDController *controller_{nullptr};
  const char *tag_;
  uint8_t *chips_;
  const uint8_t *effect_data_;
  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
  boolean do_refresh_{false};

 public:
  const uint8_t chip_channels_;
  const uint16_t num_chips_;

  uint8_t *chips() { return this->chips_; }

  const uint8_t *effect_data() { return this->effect_data_; }

  void setup() override;
  void dump_config() override;

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void schedule_refresh() { this->do_refresh_ = true; }
  void loop() override;
  void set_controller(CLEDController *controller) { this->controller_ = controller; }
};

}  // namespace fastled_bus
}  // namespace esphome

#endif
