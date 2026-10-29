#pragma once

#include <cstring>
#include <limits>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#include "event_data.h"

namespace esphome::event {

/// A codegen table in flash, or a heap copy of a runtime list (see ConstVector).
using EventTypes = ConstVector<const char *, true>;

#define LOG_EVENT(prefix, type, obj) \
  if (log_entity_header(TAG, prefix, LOG_STR_LITERAL(type), (obj))) { \
    LOG_ENTITY_ICON(TAG, prefix, *(obj)); \
    LOG_ENTITY_DEVICE_CLASS(TAG, prefix, *(obj)); \
  }

class Event : public EntityBase {
 public:
  /// Trigger an event; the type is matched against the configured types by string compare.
  void trigger(const char *event_type);
  void trigger(const std::string &event_type) { this->trigger(event_type.c_str()); }
#ifdef USE_EVENT_DATA
  /// Trigger an event with values, e.g. trigger("rotate_left", {{STEPS, 3}}). Each value must belong to a
  /// variable the event type may carry; variables without a value are missing from the event.
  void trigger(const char *event_type, std::initializer_list<EventArg> args);
  void trigger(const std::string &event_type, std::initializer_list<EventArg> args) {
    this->trigger(event_type.c_str(), args);
  }

  /// Codegen only: the event's data block and its variables in flash.
  template<size_t N> void set_data(EventDataBlock<N> *block, const EventVariable *variables, uint8_t count) {
    block->header.variables = variables;
    block->header.count = count;
    if constexpr (EVENT_DATA_SLOTS > 1)
      block->header.slot_size.value = N;
    this->data_ = &block->header;
  }
  /// The values of the last event; empty when the event declares no variables.
  EventData get_last_event_data() const { return EventData(this->data_); }
#endif

  /// Codegen only: points at a table that outlives the event (see ConstVector::assign_static).
  void set_event_types_static(const char *const *event_types, size_t count) {
    this->types_.assign_static(event_types, count);
    this->last_event_type_ = nullptr;
#ifdef USE_EVENT_DATA
    this->clear_data_();
#endif
  }
  /// Runtime lists are copied; the strings must still outlive the event.
  void set_event_types(std::initializer_list<const char *> event_types) {
    this->set_event_types_copy_(event_types.begin(), event_types.size());
  }
  /// Copy the event types of another event, for components that wrap one.
  void set_event_types(const EventTypes &event_types) {
    this->set_event_types_copy_(event_types.data(), event_types.size());
  }
  void set_event_types(const FixedVector<const char *> &event_types) {
    this->set_event_types_copy_(event_types.begin(), event_types.size());
  }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_event_types(std::initializer_list<std::string> event_types) = delete;
  void set_event_types(const FixedVector<std::string> &event_types) = delete;

  /// Return the event types supported by this event.
  const EventTypes &get_event_types() const { return this->types_; }

  /// Return the last triggered event type, or empty StringRef if no event triggered yet.
  StringRef get_last_event_type() const { return StringRef::from_maybe_nullptr(this->last_event_type_); }

  /// Return event type by index, or nullptr if index is out of bounds.
  const char *get_event_type(uint8_t index) const {
    return index < this->types_.size() ? this->types_[index] : nullptr;
  }

  /// Return index of last triggered event type, or max uint8_t if no event triggered yet.
  uint8_t get_last_event_type_index() const {
    if (this->last_event_type_ == nullptr)
      return std::numeric_limits<uint8_t>::max();
    // Most events have <3 types, uint8_t is sufficient for all reasonable scenarios
    const uint8_t size = static_cast<uint8_t>(this->types_.size());
    for (uint8_t i = 0; i < size; i++) {
      if (this->types_[i] == this->last_event_type_)
        return i;
    }
    return std::numeric_limits<uint8_t>::max();
  }

  /// Check if an event has been triggered.
  bool has_event() const { return this->last_event_type_ != nullptr; }

  template<typename F> void add_on_event_callback(F &&callback) {
    this->event_callback_.add(std::forward<F>(callback));
  }

 protected:
  void set_event_types_copy_(const char *const *event_types, size_t count);
#ifdef USE_EVENT_DATA
  void store_data_(const char *event_type, std::initializer_list<EventArg> args);
  void clear_data_() {
    if (this->data_ == nullptr)
      return;
    for (uint16_t &present : this->data_->present)
      present = 0;
  }
#endif

  LazyCallbackManager<void(StringRef event_type)> event_callback_;
  EventTypes types_;

 private:
  /// Last triggered event type - must point to entry in types_ to ensure valid lifetime.
  /// Set by trigger() after validation, reset to nullptr when types_ changes.
  const char *last_event_type_{nullptr};
#ifdef USE_EVENT_DATA
  /// Values of the last event; nullptr when the event declares no variables.
  EventDataHeader *data_{nullptr};
#endif
};

}  // namespace esphome::event
