#pragma once

#include <set>
#include <utility>
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace select {

#define LOG_SELECT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class Select;

enum SelectOp {
  SELECT_OP_NONE,
  SELECT_OP_SET,
  SELECT_OP_NEXT,
  SELECT_OP_PREVIOUS,
  SELECT_OP_FIRST,
  SELECT_OP_LAST
};

class SelectCall {
 public:
  explicit SelectCall(Select *parent) : parent_(parent) {}
  void perform();

  SelectCall &set_option(const std::string &option) {
    op_ = SELECT_OP_SET;
    option_ = option;
    return *this;
  }
  const optional<std::string> &get_option() const { return option_; }

  SelectCall &select_next(bool cycle) {
    op_ = SELECT_OP_NEXT;
    cycle_ = cycle;
    return *this;
  }

  SelectCall &select_previous(bool cycle) {
    op_ = SELECT_OP_PREVIOUS;
    cycle_ = cycle;
    return *this;
  }

  SelectCall &select_first() {
    op_ = SELECT_OP_FIRST;
    return *this;
  }

  SelectCall &select_last() {
    op_ = SELECT_OP_LAST;
    return *this;
  }

 protected:
  Select *const parent_;
  SelectOp op_{SELECT_OP_NONE};
  optional<std::string> option_;
  bool cycle_;
};

class SelectTraits {
 public:
  void set_options(std::vector<std::string> options) { this->options_ = std::move(options); }
  std::vector<std::string> get_options() const { return this->options_; }

 protected:
  std::vector<std::string> options_;
};

/** Base-class for all selects.
 *
 * A select can use publish_state to send out a new value.
 */
class Select : public EntityBase {
 public:
  std::string state;

  void publish_state(const std::string &state);

  SelectCall make_call() { return SelectCall(this); }
  void set(const std::string &value) { make_call().set_option(value).perform(); }

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

  size_t number_of_options() const;
  optional<size_t> index_of(const std::string &option) const;
  optional<std::string> option_at(const size_t index) const;

  CallbackManager<void(std::string)> state_callback_;
  bool has_state_{false};
};

}  // namespace select
}  // namespace esphome
