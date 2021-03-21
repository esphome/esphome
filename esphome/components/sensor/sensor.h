#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/filter.h"

namespace esphome {
namespace sensor {

#define LOG_SENSOR(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, obj->get_name().c_str()); \
    ESP_LOGCONFIG(TAG, "%s  Unit of Measurement: '%s'", prefix, obj->get_unit_of_measurement().c_str()); \
    ESP_LOGCONFIG(TAG, "%s  Accuracy Decimals: %d", prefix, obj->get_accuracy_decimals()); \
    if (!obj->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, obj->get_icon().c_str()); \
    } \
    if (!obj->unique_id().empty()) { \
      ESP_LOGV(TAG, "%s  Unique ID: '%s'", prefix, obj->unique_id().c_str()); \
    } \
    if (obj->get_force_update()) { \
      ESP_LOGV(TAG, "%s  Force Update: YES", prefix); \
    } \
  }

/** Base-class for all sensors.
 *
 * A sensor has unit of measurement and can use publish_state to send out a new value with the specified accuracy.
 */
class Sensor : public Nameable {
 public:
  explicit Sensor();
  explicit Sensor(const std::string &name);

  /** Manually set the unit of measurement of this sensor. By default the sensor's default defined by
   * unit_of_measurement() is used.
   *
   * @param unit_of_measurement The unit of measurement, "" to disable.
   */
  void set_unit_of_measurement(const std::string &unit_of_measurement);

  /** Manually set the icon of this sensor. By default the sensor's default defined by icon() is used.
   *
   * @param icon The icon, for example "mdi:flash". "" to disable.
   */
  void set_icon(const std::string &icon);

  /** Manually set the accuracy in decimals for this sensor. By default, the sensor's default defined by
   * accuracy_decimals() is used.
   *
   * @param accuracy_decimals The accuracy decimal that should be used.
   */
  void set_accuracy_decimals(int8_t accuracy_decimals);

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

  /// Getter-syntax for .value. Please use .state instead.
  float get_value() const ESPDEPRECATED(".value is deprecated, please use .state");
  /// Getter-syntax for .state.
  float get_state() const;
  /// Getter-syntax for .raw_value. Please use .raw_state instead.
  float get_raw_value() const ESPDEPRECATED(".raw_value is deprecated, please use .raw_state");
  /// Getter-syntax for .raw_state
  float get_raw_state() const;

  /// Get the accuracy in decimals. Uses the manual override if specified or the default value instead.
  int8_t get_accuracy_decimals();

  /// Get the unit of measurement. Uses the manual override if specified or the default value instead.
  std::string get_unit_of_measurement();

  /// Get the Home Assistant Icon. Uses the manual override if specified or the default value instead.
  std::string get_icon();

  /** Publish a new state to the front-end.
   *
   * First, the new state will be assigned to the raw_value. Then it's passed through all filters
   * until it finally lands in the .value member variable and a callback is issued.
   *
   * @param state The state as a floating point number.
   */
  void publish_state(float state);

  /** Push a new value to the MQTT front-end.
   *
   * Note: deprecated, please use publish_state.
   */
  void push_new_value(float state) ESPDEPRECATED("push_new_value is deprecated. Please use .publish_state instead");

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

  /** This member variable stores the current raw state of the sensor. Unlike .state,
   * this will be updated immediately when publish_state is called.
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

  /// Return with which interval the sensor is polled. Return 0 for non-polling mode.
  virtual uint32_t update_interval();

  /// Calculate the expected update interval for values that pass through all filters.
  uint32_t calculate_expected_filter_update_interval();

  void internal_send_state_to_frontend(float state);

  bool get_force_update() const { return force_update_; }
  /** Set this sensor's force_update mode.
   *
   * If the sensor is in force_update mode, the frontend is required to save all
   * state changes to the database when they are published, even if the state is the
   * same as before.
   */
  void set_force_update(bool force_update) { force_update_ = force_update; }

 protected:
  /** Override this to set the Home Assistant unit of measurement for this sensor.
   *
   * Return "" to disable this feature.
   *
   * @return The icon of this sensor, for example "°C".
   */
  virtual std::string unit_of_measurement();  // NOLINT

  /** Override this to set the Home Assistant icon for this sensor.
   *
   * Return "" to disable this feature.
   *
   * @return The icon of this sensor, for example "mdi:battery".
   */
  virtual std::string icon();  // NOLINT

  /// Return the accuracy in decimals for this sensor.
  virtual int8_t accuracy_decimals();  // NOLINT

  uint32_t hash_base() override;

  CallbackManager<void(float)> raw_callback_;  ///< Storage for raw state callbacks.
  CallbackManager<void(float)> callback_;      ///< Storage for filtered state callbacks.
  /// Override the unit of measurement
  optional<std::string> unit_of_measurement_;
  /// Override the icon advertised to Home Assistant, otherwise sensor's icon will be used.
  optional<std::string> icon_;
  /// Override the accuracy in decimals, otherwise the sensor's values will be used.
  optional<int8_t> accuracy_decimals_;
  Filter *filter_list_{nullptr};  ///< Store all active filters.
  bool has_state_{false};
  bool force_update_{false};
};

class PollingSensorComponent : public PollingComponent, public Sensor {
 public:
  explicit PollingSensorComponent(const std::string &name, uint32_t update_interval);

  uint32_t update_interval() override;
};

}  // namespace sensor
}  // namespace esphome
