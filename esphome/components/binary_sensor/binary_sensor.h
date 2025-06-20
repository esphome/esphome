#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/filter.h"

#include <vector>

namespace esphome {

namespace binary_sensor {

#define LOG_BINARY_SENSOR(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class().c_str()); \
    } \
  }

#define SUB_BINARY_SENSOR(name) \
 protected: \
  binary_sensor::BinarySensor *name##_binary_sensor_{nullptr}; \
\
 public: \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *binary_sensor) { \
    this->name##_binary_sensor_ = binary_sensor; \
  }

/** Base class for all binary_sensor-type classes.
 *
 * This class includes a callback that components such as MQTT can subscribe to for state changes.
 * The sub classes should notify the front-end of new states via the publish_state() method which
 * handles inverted inputs for you.
 */
class BinarySensor : public StatefulEntityBase<bool>, public EntityBase_DeviceClass {
 public:
  explicit BinarySensor(){};

  /** Publish a new state to the front-end.
   *
   * @param new_state The new state.
   */
  void publish_state(bool new_state);

  /** Publish the initial state, this will not make the callback manager send callbacks
   * and is meant only for the initial state on boot.
   *
   * @param new_state The new state.
   */
  void publish_initial_state(bool new_state);

  void add_filter(Filter *filter);
  void add_filters(const std::vector<Filter *> &filters);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void send_state_internal(bool new_state);

  /// Return whether this binary sensor has outputted a state.
  virtual bool is_status_binary_sensor() const;

  // For backward compatibility, provide an accessible property

  bool state{};

 protected:
  Filter *filter_list_{nullptr};
};

class BinarySensorInitiallyOff : public BinarySensor {
 public:
  bool has_state() const override { return true; }
};

}  // namespace binary_sensor
}  // namespace esphome
