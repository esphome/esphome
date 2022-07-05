#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "text_input_call.h"
#include "text_input_traits.h"

namespace esphome {
namespace text_input {

#define LOG_TEXT_INPUT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

//class TextInput;

/** Base-class for all text inputs.
 *
 * A text input can use publish_state to send out a new value.
 */
class TextInput : public EntityBase {
 public:
  std::string state;
  TextInputTraits traits;

  void publish_state(const std::string &state);

  /// Return whether this text input has gotten a full state yet.
  bool has_state() const { return has_state_; }

  /// Instantiate a TextInputCall object to modify this text_input component's state.
  TextInputCall make_call() { return TextInputCall(this); }

  void add_on_state_callback(std::function<void(std::string)> &&callback);

 protected:
  friend class TextInputCall;

  /** Set the value of the text input, this is a virtual method that each text input integration must implement.
   *
   * This method is called by the TextInputCall.
   *
   * @param value The value as validated by the TextInputCall.
   */
  virtual void control(const std::string &value) = 0;

  CallbackManager<void(std::string)> state_callback_;
  bool has_state_{false};
};

}  // namespace text_input
}  // namespace esphome
