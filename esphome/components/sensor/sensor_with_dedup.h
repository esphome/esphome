#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

#include <cmath>

namespace esphome::sensor {

/// Declare an optional child sensor that skips publishing a value equal to the last one.
#define SUB_SENSOR_WITH_DEDUP(name, dedup_type) \
 protected: \
  sensor::SensorWithDedup<dedup_type> name##_sensor_{}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *sensor) { this->name##_sensor_.set_sensor(sensor); }

#define LOG_SENSOR_WITH_DEDUP_SAFE(tag, name, sensor) \
  if ((sensor).has_sensor()) { \
    LOG_SENSOR(tag, name, (sensor).get_sensor()); \
  }

/// A sensor pointer that may be null plus the last value it published, stored inline.
/// Publishing does nothing when no sensor is set, and skips a value equal to the last one.
template<typename T> class SensorWithDedup {
 public:
  void set_sensor(Sensor *sens) {
    this->sens_ = sens;
    this->dedup_ = {};
  }

  void publish_state_if_not_dup(T state) {
    if (this->sens_ != nullptr && this->dedup_.next(state)) {
      this->sens_->publish_state(static_cast<float>(state));
    }
  }

  void publish_state_unknown() {
    if (this->sens_ != nullptr && this->dedup_.next_unknown()) {
      this->sens_->publish_state(NAN);
    }
  }

  bool has_sensor() const { return this->sens_ != nullptr; }
  Sensor *get_sensor() const { return this->sens_; }

 protected:
  Sensor *sens_{nullptr};
  Deduplicator<T> dedup_;
};

}  // namespace esphome::sensor
