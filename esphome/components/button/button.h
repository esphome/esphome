#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace button {

#define LOG_BUTTON(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

/** Base class for all buttons.
 *
 * A button is just a momentary switch that does not have a state, only a trigger.
 */
class Button : public EntityBase {
 public:
  explicit Button();
  explicit Button(const std::string &name);

  /** Press this button. This is called by the front-end.
   *
   * For implementing buttons, please override press_action.
   */
  void press();

  /** Set callback for state changes.
   *
   * @param callback The void() callback.
   */
  void add_on_press_callback(std::function<void()> &&callback);

  /// Set the Home Assistant device class (see button::device_class).
  void set_device_class(const std::string &device_class);

  /// Get the device class for this button.
  std::string get_device_class();

 protected:
  /** You should implement this virtual method if you want to create your own button.
   */
  virtual void press_action(){};

  uint32_t hash_base() override;

  CallbackManager<void()> press_callback_{};
  optional<std::string> device_class_{};
};

}  // namespace button
}  // namespace esphome
