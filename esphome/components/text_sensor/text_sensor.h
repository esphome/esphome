#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/filter.h"

#include <initializer_list>
#include <memory>

namespace esphome {
namespace text_sensor {

void log_text_sensor(const char *tag, const char *prefix, const char *type, TextSensor *obj);

#define LOG_TEXT_SENSOR(prefix, type, obj) log_text_sensor(TAG, prefix, LOG_STR_LITERAL(type), obj)

#define SUB_TEXT_SENSOR(name) \
 protected: \
  text_sensor::TextSensor *name##_text_sensor_{nullptr}; \
\
 public: \
  void set_##name##_text_sensor(text_sensor::TextSensor *text_sensor) { this->name##_text_sensor_ = text_sensor; }

class TextSensor : public EntityBase, public EntityBase_DeviceClass {
 public:
  std::string state;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  /// @deprecated Use get_raw_state() instead. This member will be removed in ESPHome 2026.6.0.
  ESPDEPRECATED("Use get_raw_state() instead of .raw_state. Will be removed in 2026.6.0", "2025.12.0")
  std::string raw_state;

  TextSensor() = default;
  ~TextSensor() = default;
#pragma GCC diagnostic pop

  /// Getter-syntax for .state.
  std::string get_state() const;
  /// Getter-syntax for .raw_state
  std::string get_raw_state() const;

  void publish_state(const std::string &state);

  /// Add a filter to the filter chain. Will be appended to the back.
  void add_filter(Filter *filter);

  /// Add a list of vectors to the back of the filter chain.
  void add_filters(std::initializer_list<Filter *> filters);

  /// Clear the filters and replace them by filters.
  void set_filters(std::initializer_list<Filter *> filters);

  /// Clear the entire filter chain.
  void clear_filters();

  void add_on_state_callback(std::function<void(std::string)> callback);
  /// Add a callback that will be called every time the sensor sends a raw value.
  void add_on_raw_state_callback(std::function<void(std::string)> callback);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  void internal_send_state_to_frontend(const std::string &state);

 protected:
  std::unique_ptr<CallbackManager<void(std::string)>>
      raw_callback_;                             ///< Storage for raw state callbacks (lazy allocated).
  CallbackManager<void(std::string)> callback_;  ///< Storage for filtered state callbacks.

  Filter *filter_list_{nullptr};  ///< Store all active filters.
};

}  // namespace text_sensor
}  // namespace esphome
