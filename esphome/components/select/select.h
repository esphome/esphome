#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "select_call.h"
#include "select_traits.h"

namespace esphome::select {

#define LOG_SELECT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon_ref().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon_ref().c_str()); \
    } \
  }

#define SUB_SELECT(name) \
 protected: \
  select::Select *name##_select_{nullptr}; \
\
 public: \
  void set_##name##_select(select::Select *select) { this->name##_select_ = select; }

/** Base-class for all selects.
 *
 * A select can use publish_state to send out a new value.
 */
class Select : public EntityBase {
 public:
  SelectTraits traits;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  /// @deprecated Use current_option() instead. This member will be removed in ESPHome 2026.5.0.
  ESPDEPRECATED("Use current_option() instead of .state. Will be removed in 2026.5.0", "2025.11.0")
  std::string state{};

  Select() = default;
  ~Select() = default;
#pragma GCC diagnostic pop

  void publish_state(const std::string &state);
  void publish_state(const char *state);
  void publish_state(size_t index);

  /// Return the currently selected option (as const char* from flash).
  const char *current_option() const;

  /// Instantiate a SelectCall object to modify this select component's state.
  SelectCall make_call() { return SelectCall(this); }

  /// Return whether this select component contains the provided option.
  bool has_option(const std::string &option) const;
  bool has_option(const char *option) const;

  /// Return whether this select component contains the provided index offset.
  bool has_index(size_t index) const;

  /// Return the number of options in this select component.
  size_t size() const;

  /// Find the (optional) index offset of the provided option value.
  optional<size_t> index_of(const char *option, size_t len) const;
  optional<size_t> index_of(const std::string &option) const { return this->index_of(option.data(), option.size()); }
  optional<size_t> index_of(const char *option) const { return this->index_of(option, strlen(option)); }

  /// Return the (optional) index offset of the currently active option.
  optional<size_t> active_index() const;

  /// Return the (optional) option value at the provided index offset.
  optional<std::string> at(size_t index) const;

  /// Return the option value at the provided index offset (as const char* from flash).
  const char *option_at(size_t index) const;

  void add_on_state_callback(std::function<void(std::string, size_t)> &&callback);

 protected:
  friend class SelectCall;

  size_t active_index_{0};

  /** Set the value of the select by index, this is an optional virtual method.
   *
   * IMPORTANT: At least ONE of the two control() methods must be overridden by derived classes.
   * Overriding this index-based version is PREFERRED as it avoids string conversions.
   *
   * This method is called by the SelectCall when the index is already known.
   * Default implementation converts to string and calls control(const std::string&).
   *
   * @param index The index as validated by the SelectCall.
   */
  virtual void control(size_t index) { this->control(this->option_at(index)); }

  /** Set the value of the select, this is a virtual method that each select integration can implement.
   *
   * IMPORTANT: At least ONE of the two control() methods must be overridden by derived classes.
   * Overriding control(size_t) is PREFERRED as it avoids string conversions.
   *
   * This method is called by control(size_t) when not overridden, or directly by external code.
   * Default implementation converts to index and calls control(size_t).
   *
   * @param value The value as validated by the caller.
   */
  virtual void control(const std::string &value) {
    auto index = this->index_of(value);
    if (index.has_value()) {
      this->control(index.value());
    }
  }

  CallbackManager<void(std::string, size_t)> state_callback_;
};

}  // namespace esphome::select
