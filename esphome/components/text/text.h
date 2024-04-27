#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "text_call.h"
#include "text_traits.h"

namespace esphome {
namespace text {

#define LOG_TEXT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

/** Base-class for all text inputs.
 *
 * A text input can use publish_state to send out a new value.
 */
class Text : public EntityBase {
 public:
  std::string state;
  TextTraits traits;

  void publish_state(const std::string &state);

  /// Return whether this text input has gotten a full state yet.
  bool has_state() const { return has_state_; }

  /// Instantiate a TextCall object to modify this text component's state.
  TextCall make_call() { return TextCall(this); }

  void add_on_state_callback(std::function<void(std::string)> &&callback);

 protected:
  friend class TextCall;

  /** Set the value of the text input, this is a virtual method that each text input integration must implement.
   *
   * This method is called by the TextCall.
   *
   * @param value The value as validated by the TextCall.
   */
  virtual void control(const std::string &value) = 0;

  CallbackManager<void(std::string)> state_callback_;
  bool has_state_{false};
};

}  // namespace text
}  // namespace esphome
