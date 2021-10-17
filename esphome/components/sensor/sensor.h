#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/filter.h"

namespace esphome {
namespace sensor {

#define LOG_SENSOR(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class().c_str()); \
    } \
    ESP_LOGCONFIG(TAG, "%s  State Class: '%s'", prefix, state_class_to_string((obj)->get_state_class()).c_str()); \
    ESP_LOGCONFIG(TAG, "%s  Unit of Measurement: '%s'", prefix, (obj)->get_unit_of_measurement().c_str()); \
    ESP_LOGCONFIG(TAG, "%s  Accuracy Decimals: %d", prefix, (obj)->get_accuracy_decimals()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if (!(obj)->unique_id().empty()) { \
      ESP_LOGV(TAG, "%s  Unique ID: '%s'", prefix, (obj)->unique_id().c_str()); \
    } \
    if ((obj)->get_force_update()) { \
      ESP_LOGV(TAG, "%s  Force Update: YES", prefix); \
    } \
  }

/**
 * Sensor state classes
 */
enum StateClass : uint8_t {
  STATE_CLASS_NONE = 0,
  STATE_CLASS_MEASUREMENT = 1,
  STATE_CLASS_TOTAL_INCREASING = 2,
};

std::string state_class_to_string(StateClass state_class);

/** Base-class for all sensors.
 *
 * A sensor has unit of measurement and can use publish_state to send out a new value with the specified accuracy.
 */
class Sensor : public EntityBase {
 public:
  explicit Sensor();
  explicit Sensor(const std::string &name);

  /// Get the unit of measurement, using the manual override if set.
  std::string get_unit_of_measurement();
  /// Manually set the unit of measurement.
  void set_unit_of_measurement(const std::string &unit_of_measurement);

  /// Get the accuracy in decimals, using the manual override if set.
  int8_t get_accuracy_decimals();
  /// Manually set the accuracy in decimals.
  void set_accuracy_decimals(int8_t accuracy_decimals);

  /// Get the device class, using the manual override if set.
  std::string get_device_class();
  /// Manually set the device class.
  void set_device_class(const std::string &device_class);

  /// Get the state class, using the manual override if set.
  StateClass get_state_class();
  /// Manually set the state class.
  void set_state_class(StateClass state_class);

  /**
   * Get whether force update mode is enabled.
   *
   * If the sensor is in force_update mode, the frontend is required to save all
   * state changes to the database when they are published, even if the state is the
   * same as before.
   */
  bool get_force_update() const { return force_update_; }
  /// Set force update mode.
  void set_force_update(bool force_update) { force_update_ = force_update; }

  /// Add a filter to the filter chain. Will be appended to the back.
  void add_filter(Filter *filter);

  /** Add a list of vectors to the back of the filter chain.
   *
   * This may look like:
   *
   * sensor->add_filters({
   *   LambdaFilter([&](float value) -> optional<float> { return 42/value; }),
   *   OffsetFilter(1),
   *   SlidingWindowMovingAverageFilter(15, 15), // average over last 15 values
   * });
   */
  void add_filters(const std::vector<Filter *> &filters);

  /// Clear the filters and replace them by filters.
  void set_filters(const std::vector<Filter *> &filters);

  /// Clear the entire filter chain.
  void clear_filters();

  /// Getter-syntax for .state.
  float get_state() const;
  /// Getter-syntax for .raw_state
  float get_raw_state() const;

  /** Publish a new state to the front-end.
   *
   * First, the new state will be assigned to the raw_value. Then it's passed through all filters
   * until it finally lands in the .value member variable and a callback is issued.
   *
   * @param state The state as a floating point number.
   */
  void publish_state(float state);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Add a callback that will be called every time a filtered value arrives.
  void add_on_state_callback(std::function<void(float)> &&callback);
  /// Add a callback that will be called every time the sensor sends a raw value.
  void add_on_raw_state_callback(std::function<void(float)> &&callback);

  /** This member variable stores the last state that has passed through all filters.
   *
   * On startup, when no state is available yet, this is NAN (not-a-number) and the validity
   * can be checked using has_state().
   *
   * This is exposed through a member variable for ease of use in esphome lambdas.
   */
  float state;

  /** This member variable stores the current raw state of the sensor, without any filters applied.
   *
   * Unlike .state,this will be updated immediately when publish_state is called.
   */
  float raw_state;

  /// Return whether this sensor has gotten a full state (that passed through all filters) yet.
  bool has_state() const;

  /** A unique ID for this sensor, empty for no unique id. See unique ID requirements:
   * https://developers.home-assistant.io/docs/en/entity_registry_index.html#unique-id-requirements
   *
   * @return The unique id as a string.
   */
  virtual std::string unique_id();

  void internal_send_state_to_frontend(float state);

 protected:
  /// Override this to set the default unit of measurement.
  virtual std::string unit_of_measurement();  // NOLINT

  /// Override this to set the default accuracy in decimals.
  virtual int8_t accuracy_decimals();  // NOLINT

  /// Override this to set the default device class.
  virtual std::string device_class();  // NOLINT

  /// Override this to set the default state class.
  virtual StateClass state_class();  // NOLINT

  uint32_t hash_base() override;

  CallbackManager<void(float)> raw_callback_;  ///< Storage for raw state callbacks.
  CallbackManager<void(float)> callback_;      ///< Storage for filtered state callbacks.

  bool has_state_{false};
  Filter *filter_list_{nullptr};  ///< Store all active filters.

  optional<std::string> unit_of_measurement_;           ///< Unit of measurement override
  optional<int8_t> accuracy_decimals_;                  ///< Accuracy in decimals override
  optional<std::string> device_class_;                  ///< Device class override
  optional<StateClass> state_class_{STATE_CLASS_NONE};  ///< State class override
  bool force_update_{false};                            ///< Force update mode
};

}  // namespace sensor
}  // namespace esphome
