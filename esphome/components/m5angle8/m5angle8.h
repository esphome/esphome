#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5angle8 {

static const uint8_t M5ANGLE8_REGISTER_ANALOG_INPUT_12B = 0x00;
static const uint8_t M5ANGLE8_REGISTER_ANALOG_INPUT_8B = 0x10;
static const uint8_t M5ANGLE8_REGISTER_DIGITAL_INPUT = 0x20;
static const uint8_t M5ANGLE8_REGISTER_RGB_24B = 0x30;
static const uint8_t M5ANGLE8_REGISTER_FW_VERSION = 0xFE;
static const uint8_t M5ANGLE8_REGISTER_ADDRESS = 0xFF;

static const uint8_t M5ANGLE8_NUM_KNOBS = 8;
static const uint8_t M5ANGLE8_NUM_LEDS = 9;

static const uint8_t M5ANGLE8_BYTES_PER_LED = 4;

class M5Angle8Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_sens_knob_position(int channel, sensor::Sensor *obj) { this->knob_pos_sensor_[channel] = obj; }

 protected:
  sensor::Sensor *knob_pos_sensor_[M5ANGLE8_NUM_KNOBS];
  uint8_t fw_version_;
};

class M5Angle8LightOutput : public light::AddressableLight {
 public:
  void setup() override;

  void write_state(light::LightState *state) override;

  int32_t size() const override { return M5ANGLE8_NUM_LEDS; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void set_parent(M5Angle8Component *parent) { this->parent_ = parent; }

  void clear_effect_data() override { memset(this->effect_data_, 0x00, M5ANGLE8_NUM_LEDS); }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    size_t pos = index * M5ANGLE8_BYTES_PER_LED;
    // red,green, blue, white, effect_data, color_correction
    return {this->buf_ + pos, this->buf_ + pos + 1,       this->buf_ + pos + 2,
            nullptr,          this->effect_data_ + index, &this->correction_};
  }

  M5Angle8Component *parent_{nullptr};

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
};

}  // namespace m5angle8
}  // namespace esphome
