#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/filter.h"

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
  void add_filters(std::initializer_list<Filter *> filters);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void send_state_internal(bool new_state);

  /// Return whether this binary sensor has outputted a state.
  virtual bool is_status_binary_sensor() const;

  // For backward compatibility, provide an accessible property

  bool state{};

 protected:
  Filter *filter_list_{nullptr};

  bool set_new_state(const optional<bool> &new_state) override;
};

class BinarySensorInitiallyOff : public BinarySensor {
 public:
  bool has_state() const override { return true; }
};

}  // namespace esphome::binary_sensor
