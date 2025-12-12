#pragma once

#include "esphome/core/defines.h"

#include <memory>

#ifdef USE_SENSOR
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

#define SUB_SENSOR_WITH_DEDUP(name, dedup_type) \
 protected: \
  ld24xx::SensorWithDedup<dedup_type> *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *sensor) { \
    this->name##_sensor_ = new ld24xx::SensorWithDedup<dedup_type>(sensor); \
  }
#endif

#define LOG_SENSOR_WITH_DEDUP_SAFE(tag, name, sensor) \
  if ((sensor) != nullptr) { \
    LOG_SENSOR(tag, name, (sensor)->sens); \
  }

#define SAFE_PUBLISH_SENSOR(sensor, value) \
  if ((sensor) != nullptr) { \
    (sensor)->publish_state_if_not_dup(value); \
  }

#define SAFE_PUBLISH_SENSOR_UNKNOWN(sensor) \
  if ((sensor) != nullptr) { \
    (sensor)->publish_state_unknown(); \
  }

#define highbyte(val) (uint8_t)((val) >> 8)
#define lowbyte(val) (uint8_t)((val) &0xff)

namespace esphome {
namespace ld24xx {

#ifdef USE_SENSOR
// Helper class to store a sensor with a deduplicator & publish state only when the value changes
template<typename T> class SensorWithDedup {
 public:
  SensorWithDedup(sensor::Sensor *sens) : sens(sens) {}

  void publish_state_if_not_dup(T state) {
    if (this->publish_dedup.next(state)) {
      this->sens->publish_state(static_cast<float>(state));
    }
  }

  void publish_state_unknown() {
    if (this->publish_dedup.next_unknown()) {
      this->sens->publish_state(NAN);
    }
  }

  sensor::Sensor *sens;
  Deduplicator<T> publish_dedup;
};
#endif
}  // namespace ld24xx
}  // namespace esphome
