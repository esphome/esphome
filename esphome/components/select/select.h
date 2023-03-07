#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "select_call.h"
#include "select_traits.h"

namespace esphome {
namespace select {

#define LOG_SELECT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

/** Base-class for all selects.
 *
 * A select can use publish_state to send out a new value.
 */
class Select : public EntityBase {
 public:
  std::string state;
  SelectTraits traits;

  void publish_state(const std::string &state);

  /// Return whether this select component has gotten a full state yet.
  bool has_state() const { return has_state_; }

  /// Instantiate a SelectCall object to modify this select component's state.
  SelectCall make_call() { return SelectCall(this); }

  /// Return whether this select component contains the provided option.
  bool has_option(const std::string &option) const;

  /// Return whether this select component contains the provided index offset.
  bool has_index(size_t index) const;

  /// Return the number of options in this select component.
  size_t size() const;

  /// Find the (optional) index offset of the provided option value.
  optional<size_t> index_of(const std::string &option) const;

  /// Return the (optional) index offset of the currently active option.
  optional<size_t> active_index() const;

  /// Return the (optional) option value at the provided index offset.
  optional<std::string> at(size_t index) const;

  void add_on_state_callback(std::function<void(std::string, size_t)> &&callback);

 protected:
  friend class SelectCall;

  /** Set the value of the select, this is a virtual method that each select integration must implement.
   *
   * This method is called by the SelectCall.
   *
   * @param value The value as validated by the SelectCall.
   */
  virtual void control(const std::string &value) = 0;

  CallbackManager<void(std::string, size_t)> state_callback_;
  bool has_state_{false};
};

}  // namespace select
}  // namespace esphome
