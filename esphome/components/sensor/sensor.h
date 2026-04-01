#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_SENSOR_FILTER
#include "esphome/components/sensor/filter.h"
#endif

#include <initializer_list>
#include <memory>

namespace esphome::sensor {

class Sensor;

void log_sensor(const char *tag, const char *prefix, const char *type, Sensor *obj);

#define LOG_SENSOR(prefix, type, obj) log_sensor(TAG, prefix, LOG_STR_LITERAL(type), obj)

#define SUB_SENSOR(name) \
 protected: \
  sensor::Sensor *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *sensor) { this->name##_sensor_ = sensor; }

/**
 * Sensor state classes
 */
enum StateClass : uint8_t {
  STATE_CLASS_NONE = 0,
  STATE_CLASS_MEASUREMENT = 1,
  STATE_CLASS_TOTAL_INCREASING = 2,
  STATE_CLASS_TOTAL = 3,
  STATE_CLASS_MEASUREMENT_ANGLE = 4
};
constexpr uint8_t STATE_CLASS_LAST = static_cast<uint8_t>(STATE_CLASS_MEASUREMENT_ANGLE);

const LogString *state_class_to_string(StateClass state_class);

/** Base-class for all sensors.
 *
 * A sensor has unit of measurement and can use publish_state to send out a new value with the specified accuracy.
 */
class Sensor : public EntityBase {
 public:
  explicit Sensor();

  /// Get the accuracy in decimals, using the manual override if set.
  int8_t get_accuracy_decimals();
  /// Manually set the accuracy in decimals.
  void set_accuracy_decimals(int8_t accuracy_decimals);
  /// Check if the accuracy in decimals has been manually set.
  bool has_accuracy_decimals() const { return this->sensor_flags_.has_accuracy_override; }

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
  bool get_force_update() const { return sensor_flags_.force_update; }
  /// Set force update mode.
  void set_force_update(bool force_update) { sensor_flags_.force_update = force_update; }

#ifdef USE_SENSOR_FILTER
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
  void add_filters(std::initializer_list<Filter *> filters);

  /// Clear the filters and replace them by filters.
  void set_filters(std::initializer_list<Filter *> filters);

  /// Clear the entire filter chain.
  void clear_filters();
#endif

  /// Getter-syntax for .state.
  float get_state() const { return this->state; }
  /// Getter-syntax for .raw_state
  float get_raw_state() const {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return this->raw_state;
#pragma GCC diagnostic pop
  }

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
  template<typename F> void add_on_state_callback(F &&callback) { this->callback_.add(std::forward<F>(callback)); }
  /// Add a callback that will be called every time the sensor sends a raw value.
  /// When USE_SENSOR_FILTER is not enabled, delegates to the regular callback
  /// since raw state equals filtered state without filter support compiled in.
  template<typename F> void add_on_raw_state_callback(F &&callback) {
#ifdef USE_SENSOR_FILTER
    this->raw_callback_.add(std::forward<F>(callback));
#else
    this->callback_.add(std::forward<F>(callback));
#endif
  }

  /** This member variable stores the last state that has passed through all filters.
   *
   * On startup, when no state is available yet, this is NAN (not-a-number) and the validity
   * can be checked using has_state().
   *
   * This is exposed through a member variable for ease of use in esphome lambdas.
   */
  float state;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  /// @deprecated Use get_raw_state() instead. This member will be removed in ESPHome 2026.10.0.
  ESPDEPRECATED("Use get_raw_state() instead of .raw_state. Will be removed in 2026.10.0", "2026.4.0")
  float raw_state;
#pragma GCC diagnostic pop

  void internal_send_state_to_frontend(float state);

 protected:
#ifdef USE_SENSOR_FILTER
  LazyCallbackManager<void(float)> raw_callback_;  ///< Storage for raw state callbacks.
#endif
  LazyCallbackManager<void(float)> callback_;  ///< Storage for filtered state callbacks.

#ifdef USE_SENSOR_FILTER
  Filter *filter_list_{nullptr};  ///< Store all active filters.
#endif

  // Group small members together to avoid padding
  int8_t accuracy_decimals_{-1};              ///< Accuracy in decimals (-1 = not set)
  StateClass state_class_{STATE_CLASS_NONE};  ///< State class (STATE_CLASS_NONE = not set)

  // Bit-packed flags for sensor-specific settings
  struct SensorFlags {
    uint8_t has_accuracy_override : 1;
    uint8_t has_state_class_override : 1;
    uint8_t force_update : 1;
    uint8_t reserved : 5;  // Reserved for future use
  } sensor_flags_{};
};

}  // namespace esphome::sensor
