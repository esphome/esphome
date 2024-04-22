#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

#include "Adafruit_NeoKey_1x4.h"
#include "seesaw_neopixel.h"

namespace esphome {
namespace neokey {

static const uint8_t NUM_BYTES_PER_LED = 3;

static const char *const TAG = "neokey";

class KeyListener {
 public:
  virtual void keys_update(uint8_t keys){};
};

class NeoKeyLight;

class NeoKeyComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Binary Sensors
  void register_listener(KeyListener *listener) { this->listeners_.push_back(listener); }

 protected:
  // Light
  friend NeoKeyLight;
  Adafruit_NeoKey_1x4 hub_;
  std::vector<KeyListener *> listeners_{};
};

}  // namespace neokey
}  // namespace esphome
