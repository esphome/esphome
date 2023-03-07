#pragma once
#include "gmock/gmock.h"
#include "esphome/core/entity_base.h"

namespace esphome {
namespace test {

struct Sensor : public EntityBase {
  virtual void add_on_state_callback(std::function<void(float)> &&callback) { this->callback_ = callback; }
  void set(float value) {
    state = value;
    if (callback_) {
      callback_(value);
    }
  }

  float state{0};

 protected:
  std::function<void(float)> callback_;
};

struct MockSensor : public Sensor {
  MockSensor() {
    ON_CALL(*this, add_on_state_callback).WillByDefault([this](std::function<void(bool)> &&callback) {
      return Sensor::add_on_state_callback(std::move(callback));
    });
  }
  MOCK_METHOD(void, add_on_state_callback, (std::function<void(float)> && callback), (override));
};

}  // namespace test
}  // namespace esphome
