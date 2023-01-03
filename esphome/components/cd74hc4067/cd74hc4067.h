#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace cd74hc4067 {

class CD74HC4067Component : public Component {
 public:
  /// Set up the internal sensor array.
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// setting pin active by setting the right combination of the four multiplexer input pins
  void activate_pin(uint8_t pin);

  /// set the pin connected to multiplexer control pin 0
  void set_pin_s0(GPIOPin *pin) { this->pin_s0_ = pin; }
  /// set the pin connected to multiplexer control pin 1
  void set_pin_s1(GPIOPin *pin) { this->pin_s1_ = pin; }
  /// set the pin connected to multiplexer control pin 2
  void set_pin_s2(GPIOPin *pin) { this->pin_s2_ = pin; }
  /// set the pin connected to multiplexer control pin 3
  void set_pin_s3(GPIOPin *pin) { this->pin_s3_ = pin; }

  /// set the delay needed after an input switch
  void set_switch_delay(uint32_t switch_delay) { this->switch_delay_ = switch_delay; }

 private:
  GPIOPin *pin_s0_;
  GPIOPin *pin_s1_;
  GPIOPin *pin_s2_;
  GPIOPin *pin_s3_;
  /// the currently active pin
  uint8_t active_pin_;
  uint32_t switch_delay_;
};

class CD74HC4067Sensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
 public:
  CD74HC4067Sensor(CD74HC4067Component *parent);

  void update() override;

  void dump_config() override;
  /// `HARDWARE_LATE` setup priority.
  float get_setup_priority() const override;
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_source(voltage_sampler::VoltageSampler *source) { this->source_ = source; }

  float sample() override;

 protected:
  CD74HC4067Component *parent_;
  /// The sampling source to read values from.
  voltage_sampler::VoltageSampler *source_;

  uint8_t pin_;
};
}  // namespace cd74hc4067
}  // namespace esphome
