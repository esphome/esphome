#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5stack_8angle {

static const uint8_t M5STACK_8ANGLE_REGISTER_ANALOG_INPUT_12B = 0x00;
static const uint8_t M5STACK_8ANGLE_REGISTER_ANALOG_INPUT_8B = 0x10;
static const uint8_t M5STACK_8ANGLE_REGISTER_DIGITAL_INPUT = 0x20;
static const uint8_t M5STACK_8ANGLE_REGISTER_RGB_24B = 0x30;
static const uint8_t M5STACK_8ANGLE_REGISTER_FW_VERSION = 0xFE;
static const uint8_t M5STACK_8ANGLE_REGISTER_ADDRESS = 0xFF;

static const uint8_t M5STACK_8ANGLE_NUM_KNOBS = 8;
static const uint8_t M5STACK_8ANGLE_NUM_LEDS = 9;

static const uint8_t M5STACK_8ANGLE_BYTES_PER_LED = 4;

class M5Stack_8AngleComponent : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float read_knob_pos(uint8_t channel);
  int read_switch();

 protected:
  uint8_t fw_version_;
};

#ifdef USE_LIGHT
class M5Stack_8AngleLightOutput : public light::AddressableLight {
 public:
  void setup() override;

  void write_state(light::LightState *state) override;

  int32_t size() const override { return M5STACK_8ANGLE_NUM_LEDS; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  };

  void set_parent(M5Stack_8AngleComponent *parent) { this->parent_ = parent; };

  M5Stack_8AngleComponent *get_parent() { return this->parent_; };

  void clear_effect_data() override { memset(this->effect_data_, 0x00, M5STACK_8ANGLE_NUM_LEDS); };

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    size_t pos = index * M5STACK_8ANGLE_BYTES_PER_LED;
    // red,green, blue, white, effect_data, color_correction
    return {this->buf_ + pos, this->buf_ + pos + 1,       this->buf_ + pos + 2,
            nullptr,          this->effect_data_ + index, &this->correction_};
  }

  M5Stack_8AngleComponent *parent_{nullptr};

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
};
#endif

#ifdef USE_SENSOR
class M5Stack_8AngleSensorKnob : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void set_parent(M5Stack_8AngleComponent *parent, uint8_t index) {
    this->parent_ = parent;
    this->knob_index_ = index;
  };
  M5Stack_8AngleComponent *get_parent() { return this->parent_; };

 protected:
  M5Stack_8AngleComponent *parent_{nullptr};
  uint8_t knob_index_ = 0;
};
#endif

#ifdef USE_BINARY_SENSOR
class M5Stack_8AngleSensorSwitch : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  void update() override;
  void set_parent(M5Stack_8AngleComponent *parent) { this->parent_ = parent; };
  M5Stack_8AngleComponent *get_parent() { return this->parent_; };

 protected:
  M5Stack_8AngleComponent *parent_{nullptr};
};
#endif

}  // namespace m5stack_8angle
}  // namespace esphome
