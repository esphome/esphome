#pragma once

#include "esphome/core/component.h"
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
class BinarySensor : public EntityBase, public EntityBase_DeviceClass {
 public:
  explicit BinarySensor();

  /** Add a callback to be notified of state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void(bool)> &&callback);

  /** Publish a new state to the front-end.
   *
   * @param state The new state.
   */
  void publish_state(bool state);

  /** Publish the initial state, this will not make the callback manager send callbacks
   * and is meant only for the initial state on boot.
   *
   * @param state The new state.
   */
  void publish_initial_state(bool state);

  /// The current reported state of the binary sensor.
  bool state;

  void add_filter(Filter *filter);
  void add_filters(const std::vector<Filter *> &filters);

  void set_publish_initial_state(bool publish_initial_state) { this->publish_initial_state_ = publish_initial_state; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void send_state_internal(bool state, bool is_initial);

  /// Return whether this binary sensor has outputted a state.
  virtual bool has_state() const;

  virtual bool is_status_binary_sensor() const;

 protected:
  CallbackManager<void(bool)> state_callback_{};
  Filter *filter_list_{nullptr};
  bool has_state_{false};
  bool publish_initial_state_{false};
  Deduplicator<bool> publish_dedup_;
};

class BinarySensorInitiallyOff : public BinarySensor {
 public:
  bool has_state() const override { return true; }
};

}  // namespace binary_sensor
}  // namespace esphome
