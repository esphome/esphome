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

#define SUB_BUTTON(name) \
 protected: \
  button::Button *name##_button_{nullptr}; \
\
 public: \
  void set_##name##_button(button::Button *button) { this->name##_button_ = button; }

/** Base class for all buttons.
 *
 * A button is just a momentary switch that does not have a state, only a trigger.
 */
class Button : public EntityBase, public EntityBase_DeviceClass {
 public:
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

 protected:
  /** You should implement this virtual method if you want to create your own button.
   */
  virtual void press_action() = 0;

  CallbackManager<void()> press_callback_{};
};

}  // namespace button
}  // namespace esphome
