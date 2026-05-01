#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#ifdef USE_BINARY_SENSOR_FILTER
#include "esphome/components/binary_sensor/filter.h"
#endif

#include <initializer_list>

namespace esphome::binary_sensor {

class BinarySensor;
void log_binary_sensor(const char *tag, const char *prefix, const char *type, BinarySensor *obj);

#define LOG_BINARY_SENSOR(prefix, type, obj) log_binary_sensor(TAG, prefix, LOG_STR_LITERAL(type), obj)

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
class BinarySensor : public StatefulEntityBase<bool> {
 public:
  explicit BinarySensor() = default;

  const bool &get_state() const override { return this->state; }
  void set_trigger_on_initial_state(bool value) { this->trigger_on_initial_state_ = value; }

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

#ifdef USE_BINARY_SENSOR_FILTER
  void add_filter(Filter *filter);
  void add_filters(std::initializer_list<Filter *> filters);
#endif

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void send_state_internal(bool new_state) {
    // Fast path: skip virtual dispatch when state hasn't changed
    if (this->flags_.has_state && this->state == new_state)
      return;
    this->set_new_state(new_state);
  }

  /// Return whether this binary sensor has outputted a state.
  virtual bool is_status_binary_sensor() const;

  /// The current state of this binary sensor. Also used as the backing storage for StatefulEntityBase.
  bool state{};

 protected:
  bool get_trigger_on_initial_state() const override { return this->trigger_on_initial_state_; }
  void set_state_value(const bool &value) override { this->state = value; }

  bool trigger_on_initial_state_{true};
#ifdef USE_BINARY_SENSOR_FILTER
  Filter *filter_list_{nullptr};
#endif

  bool set_new_state(const optional<bool> &new_state) override;
};

class BinarySensorInitiallyOff : public BinarySensor {
 public:
  BinarySensorInitiallyOff() { this->set_has_state(true); }
};

}  // namespace esphome::binary_sensor
