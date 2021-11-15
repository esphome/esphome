#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/af4991/af4991.h"

namespace esphome {
namespace af4991 {

class AF4991Sensor : public Component, public sensor::Sensor {
 public:
  // Set parent value of the sensor
  void set_parent(AF4991 *parent) { this->parent_ = parent; }

  // Set the sensor value
  void set_sensor_value(int32_t value) {
    this->parent_->write_32(
        (uint16_t) adafruit_seesaw::SEESAW_ENCODER_BASE << 8 | adafruit_seesaw::SEESAW_ENCODER_POSITION,
        (uint32_t) value);
  }

  // Invert sensor value
  void set_sensor_invert(bool invert) { this->invert_ = invert; }

  // Publish initial value
  void set_publish_initial_value(bool publish_initial_value) { this->publish_initial_value_ = publish_initial_value; }

  // Set min and max values
  void set_min_value(int32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(int32_t max_value) { this->max_value_ = max_value; }

  // Callbacks trigger after x number of steps
  void set_clockwise_steps_before_trigger(int32_t value) { this->clockwise_steps_before_trigger_ = value; }
  void set_anticlockwise_steps_before_trigger(int32_t value) { this->anticlockwise_steps_before_trigger_ = value; }

  // Directional callbacks
  void add_on_clockwise_callback(std::function<void()> callback) {
    this->on_clockwise_callback_.add(std::move(callback));
  }
  void add_on_anticlockwise_callback(std::function<void()> callback) {
    this->on_anticlockwise_callback_.add(std::move(callback));
  }

  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  int32_t check_position_within_limit_(int32_t position);

  AF4991 *parent_{nullptr};

  bool invert_ = false;
  bool publish_initial_value_ = false;

  int32_t min_value_{INT32_MIN};
  int32_t max_value_{INT32_MAX};

  volatile int32_t position_{this->min_value_ > 0 ? this->min_value_ : 0};
  int32_t clockwise_steps_before_trigger_{1};
  int32_t anticlockwise_steps_before_trigger_{1};
  int32_t clockwise_move_{0};
  int32_t anticlockwise_move_{0};

  CallbackManager<void()> on_clockwise_callback_;
  CallbackManager<void()> on_anticlockwise_callback_;
};

template<typename... Ts> class AF4991SensorSetValueAction : public Action<Ts...> {
 public:
  AF4991SensorSetValueAction(AF4991Sensor *encoder) : encoder_(encoder) {}
  TEMPLATABLE_VALUE(int32_t, value)

  void play(Ts... x) override { this->encoder_->set_sensor_value((int32_t) this->value_.value(x...)); }

 protected:
  AF4991Sensor *encoder_;
};

class AF4991SensorClockwiseTrigger : public Trigger<> {
 public:
  explicit AF4991SensorClockwiseTrigger(AF4991Sensor *parent) {
    parent->add_on_clockwise_callback([this]() { this->trigger(); });
  }
};

class AF4991SensorAnticlockwiseTrigger : public Trigger<> {
 public:
  explicit AF4991SensorAnticlockwiseTrigger(AF4991Sensor *parent) {
    parent->add_on_anticlockwise_callback([this]() { this->trigger(); });
  }
};

}  // namespace af4991
}  // namespace esphome
