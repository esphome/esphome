#pragma once

#include <set>
#include <utility>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace select {

#define LOG_SELECT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, (obj)->get_name().c_str()); \
    if (!(obj)->traits.get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->traits.get_icon().c_str()); \
    } \
  }

class Select;

class SelectCall {
 public:
  explicit SelectCall(Select *parent) : parent_(parent) {}
  void perform();

  SelectCall &set_value(const std::string &value) {
    value_ = value;
    return *this;
  }
  const optional<std::string> &get_value() const { return value_; }

 protected:
  Select *const parent_;
  optional<std::string> value_;
};

class SelectTraits {
 public:
  void set_options(std::vector<std::string> options) { this->options_ = std::move(options); }
  const std::vector<std::string> get_options() const { return this->options_; }
  void set_icon(std::string icon) { icon_ = std::move(icon); }
  const std::string &get_icon() const { return icon_; }

 protected:
  std::vector<std::string> options_;
  std::string icon_;
};

/** Base-class for all selects.
 *
 * A select can use publish_state to send out a new value.
 */
class Select : public Nameable {
 public:
  std::string state;

  void publish_state(const std::string &state);

  SelectCall make_call() { return SelectCall(this); }
  void set(const std::string &value) { make_call().set_value(value).perform(); }

  void add_on_state_callback(std::function<void(std::string)> &&callback);

  SelectTraits traits;

  /// Return whether this select has gotten a full state yet.
  bool has_state() const { return has_state_; }

 protected:
  friend class SelectCall;

  /** Set the value of the select, this is a virtual method that each select integration must implement.
   *
   * This method is called by the SelectCall.
   *
   * @param value The value as validated by the SelectCall.
   */
  virtual void control(const std::string &value) = 0;

  uint32_t hash_base() override;

  CallbackManager<void(std::string)> state_callback_;
  bool has_state_{false};
};

}  // namespace select
}  // namespace esphome
