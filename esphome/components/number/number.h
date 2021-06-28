#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace number {

#define LOG_NUMBER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if (!(obj)->unique_id().empty()) { \
      ESP_LOGV(TAG, "%s  Unique ID: '%s'", prefix, (obj)->unique_id().c_str()); \
    } \
  }

/** Base-class for all numbers.
 *
 * A number can use publish_state to send out a new value.
 */
class Number : public Nameable {
 public:
  explicit Number();
  explicit Number(const std::string &name);

  /** Manually set the icon of this number. By default the number's default defined by icon() is used.
   *
   * @param icon The icon, for example "mdi:flash". "" to disable.
   */
  void set_icon(const std::string &icon);

  /// Getter-syntax for .state.
  float get_state() const;

  /// Get the accuracy in decimals. Based on the step value.
  int8_t get_accuracy_decimals();

  /// Get the Home Assistant Icon. Uses the manual override if specified or the default value instead.
  std::string get_icon();

  /** Publish a new state to the front-end.
   *
   * @param state The state as a floating point number.
   */
  void publish_state(float state);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Add a callback that will be called every time a filtered value arrives.
  void add_on_state_callback(std::function<void(float)> &&callback);

  /** This member variable stores the last state that has passed through all filters.
   *
   * On startup, when no state is available yet, this is NAN (not-a-number) and the validity
   * can be checked using has_state().
   *
   * This is exposed through a member variable for ease of use in esphome lambdas.
   */
  float state;

  /// Return whether this number has gotten a full state (that passed through all filters) yet.
  bool has_state() const;

  /** A unique ID for this number, empty for no unique id. See unique ID requirements:
   * https://developers.home-assistant.io/docs/en/entity_registry_index.html#unique-id-requirements
   *
   * @return The unique id as a string.
   */
  virtual std::string unique_id();

  /// Return with which interval the number is polled. Return 0 for non-polling mode.
  virtual uint32_t update_interval();

  void set_min_value(float min_value) { this->min_value_ = min_value; }
  void set_max_value(float max_value) { this->max_value_ = max_value; }
  void set_step(float step) { this->step_ = step; }

  float min_value() const { return this->min_value_; }
  float max_value() const { return this->max_value_; }
  float step() const { return this->step_; }

 protected:
  /** Override this to set the Home Assistant icon for this number.
   *
   * Return "" to disable this feature.
   *
   * @return The icon of this number, for example "mdi:battery".
   */
  virtual std::string icon();  // NOLINT

  uint32_t hash_base() override;

  CallbackManager<void(float)> callback_;  ///< Storage for filtered state callbacks.
  /// Override the icon advertised to Home Assistant, otherwise number's icon will be used.
  optional<std::string> icon_;
  bool has_state_{false};
  float step_{1.0};
  float min_value_{0};
  float max_value_{100};
};

class PollingNumberComponent : public PollingComponent, public Number {
 public:
  explicit PollingNumberComponent(const std::string &name, uint32_t update_interval);

  uint32_t update_interval() override;
};

}  // namespace number
}  // namespace esphome
