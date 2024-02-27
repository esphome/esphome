#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/filter.h"

#include <vector>

namespace esphome {
namespace text_sensor {

#define LOG_TEXT_SENSOR(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class().c_str()); \
    } \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if (!(obj)->unique_id().empty()) { \
      ESP_LOGV(TAG, "%s  Unique ID: '%s'", prefix, (obj)->unique_id().c_str()); \
    } \
  }

#define SUB_TEXT_SENSOR(name) \
 protected: \
  text_sensor::TextSensor *name##_text_sensor_{nullptr}; \
\
 public: \
  void set_##name##_text_sensor(text_sensor::TextSensor *text_sensor) { this->name##_text_sensor_ = text_sensor; }

class TextSensor : public EntityBase, public EntityBase_DeviceClass {
 public:
  /// Getter-syntax for .state.
  std::string get_state() const;
  /// Getter-syntax for .raw_state
  std::string get_raw_state() const;

  void publish_state(const std::string &state);

  /// Add a filter to the filter chain. Will be appended to the back.
  void add_filter(Filter *filter);

  /// Add a list of vectors to the back of the filter chain.
  void add_filters(const std::vector<Filter *> &filters);

  /// Clear the filters and replace them by filters.
  void set_filters(const std::vector<Filter *> &filters);

  /// Clear the entire filter chain.
  void clear_filters();

  void add_on_state_callback(std::function<void(std::string)> callback);
  /// Add a callback that will be called every time the sensor sends a raw value.
  void add_on_raw_state_callback(std::function<void(std::string)> callback);

  std::string state;
  std::string raw_state;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /** Override this method to set the unique ID of this sensor.
   *
   * @deprecated Do not use for new sensors, a suitable unique ID is automatically generated (2023.4).
   */
  virtual std::string unique_id();

  bool has_state();

  void internal_send_state_to_frontend(const std::string &state);

 protected:
  CallbackManager<void(std::string)> raw_callback_;  ///< Storage for raw state callbacks.
  CallbackManager<void(std::string)> callback_;      ///< Storage for filtered state callbacks.

  Filter *filter_list_{nullptr};  ///< Store all active filters.

  bool has_state_{false};
};

}  // namespace text_sensor
}  // namespace esphome
