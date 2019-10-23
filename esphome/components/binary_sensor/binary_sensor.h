#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/filter.h"

namespace esphome {

namespace binary_sensor {

#define LOG_BINARY_SENSOR(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, obj->get_name().c_str()); \
    if (!obj->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, obj->get_device_class().c_str()); \
    } \
  }

/** Base class for all binary_sensor-type classes.
 *
 * This class includes a callback that components such as MQTT can subscribe to for state changes.
 * The sub classes should notify the front-end of new states via the publish_state() method which
 * handles inverted inputs for you.
 */
class BinarySensor : public Nameable {
 public:
  explicit BinarySensor();
  /** Construct a binary sensor with the specified name
   *
   * @param name Name of this binary sensor.
   */
  explicit BinarySensor(const std::string &name);

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

  /// Manually set the Home Assistant device class (see binary_sensor::device_class)
  void set_device_class(const std::string &device_class);

  /// Get the device class for this binary sensor, using the manual override if specified.
  std::string get_device_class();

  void add_filter(Filter *filter);
  void add_filters(std::vector<Filter *> filters);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void send_state_internal(bool state, bool is_initial);

  /// Return whether this binary sensor has outputted a state.
  bool has_state() const;

  virtual bool is_status_binary_sensor() const;

  // ========== OVERRIDE METHODS ==========
  // (You'll only need this when creating your own custom binary sensor)
  /// Get the default device class for this sensor, or empty string for no default.
  virtual std::string device_class();

 protected:
  uint32_t hash_base() override;

  CallbackManager<void(bool)> state_callback_{};
  optional<std::string> device_class_{};  ///< Stores the override of the device class
  Filter *filter_list_{nullptr};
  bool has_state_{false};
  Deduplicator<bool> publish_dedup_;
};

}  // namespace binary_sensor
}  // namespace esphome
