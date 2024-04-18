#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_LIGHT
#include "esphome/components/light/addressable_light.h"
#endif

#include "Adafruit_NeoKey_1x4.h"
#include "seesaw_neopixel.h"

namespace esphome {
namespace neokey {

#define NUM_BYTES_PER_LED 3

static const char *const TAG = "neokey";

class KeyListener {
 public:
  virtual void keys_update(uint8_t keys){};
};

#ifdef USE_BINARY_SENSOR
class NeoKeyBinarySensor : public binary_sensor::BinarySensor, public KeyListener {
 public:
  void set_key(uint8_t key) { key_ = key; };
  void keys_update(uint8_t keys) override;
 protected:
  uint8_t key_{0};
};
#endif

#ifdef USE_LIGHT
class NeoKeyLight;
#endif

class NeoKeyComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Binary Sensors
  void register_listener(KeyListener *listener) { this->listeners_.push_back(listener); }

 protected:
#ifdef USE_LIGHT
  friend NeoKeyLight;
#endif
  Adafruit_NeoKey_1x4 hub_;
  std::vector<KeyListener *> listeners_{}; 
};

#ifdef USE_LIGHT
class NeoKeyLight : public light::AddressableLight {
 public:
  void setup() override;
  void set_neokey(NeoKeyComponent *neokey) { neokey_ = neokey; }
  int32_t size() const override { return this->neokey_ == nullptr ? 0 : this->neokey_->hub_.pixels.numPixels(); }
  void write_state(light::LightState *state) override;
  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
 protected:
  NeoKeyComponent *neokey_;

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};

  light::ESPColorView get_view_internal(int32_t index) const override {
    uint8_t *base = this->buf_ + (NUM_BYTES_PER_LED * index);

    return light::ESPColorView(base + 0, base + 1, base + 2, nullptr, this->effect_data_ + index, &this->correction_);
  }
};
#endif

}  // namespace neokey
}  // namespace esphome