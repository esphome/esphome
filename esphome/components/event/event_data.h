#pragma once

#include "esphome/core/defines.h"

#ifdef USE_EVENT_DATA

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "esphome/core/hal.h"
#include "esphome/core/string_ref.h"

namespace esphome::event {

/// Type of an event variable; the numbers match the API's EventVariableType.
enum EventVariableType : uint8_t {
  EVENT_VARIABLE_TYPE_BOOL = 0,
  EVENT_VARIABLE_TYPE_INT = 1,
  EVENT_VARIABLE_TYPE_FLOAT = 2,
  EVENT_VARIABLE_TYPE_STRING = 3,
};

/// An event declares at most this many variables: one bit each in the presence mask.
static constexpr uint8_t EVENT_MAX_VARIABLES = 16;
/// An event with variables has at most this many event types: the API keeps the type index of a
/// queued event in 7 bits.
static constexpr uint8_t EVENT_DATA_MAX_TYPES = 127;
/// A text value has at most this many bytes, so the API sends its length in one byte.
static constexpr uint8_t EVENT_MAX_STRING_LENGTH = 127;

/// One declared variable. Codegen keeps the table in flash; read an entry with EventData::variable().
struct EventVariable {
  const char *name;  ///< in flash on ESP8266
  /// Names of the event types that may carry the variable, ending with nullptr; nullptr: every type may.
  const char *const *event_types;
  uint16_t offset;     ///< of the value in the values of the data block
  uint8_t type;        ///< EventVariableType
  uint8_t max_length;  ///< longest text in bytes; 0 for the other types
};

/// Integers whose values all fit an int32_t, and enums of at most 32 bits. A wider integer, such as a uint32_t,
/// must be cast, so that its value is never cut without notice. An enum is converted as is.
template<typename T>
static constexpr bool EVENT_FITS_INT32 =
    (std::is_enum_v<T> && sizeof(T) <= sizeof(int32_t)) ||
    (std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= sizeof(int32_t) &&
     (std::is_signed_v<T> || sizeof(T) < sizeof(int32_t)));

/// A value passed to Event::trigger(): a bool, an integer that fits an int32_t, a float or a text.
/// Its type must match the variable; an integer is also accepted for a float variable.
/// A text is not copied until trigger() stores it, so it only has to live until then.
struct EventValue {
  // Only a bool itself, so that a pointer or a wide integer does not turn into one
  template<typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
  constexpr EventValue(T value) : type(EVENT_VARIABLE_TYPE_BOOL), bool_value(value) {}
  template<typename T, std::enable_if_t<EVENT_FITS_INT32<T>, bool> = true>
  constexpr EventValue(T value) : type(EVENT_VARIABLE_TYPE_INT), int_value(static_cast<int32_t>(value)) {}
  template<typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
  constexpr EventValue(T value) : type(EVENT_VARIABLE_TYPE_FLOAT), float_value(static_cast<float>(value)) {}
  constexpr EventValue(StringRef value) : type(EVENT_VARIABLE_TYPE_STRING), string_value(value) {}
  EventValue(const char *value) : EventValue(StringRef::from_maybe_nullptr(value)) {}
  EventValue(const std::string &value) : EventValue(StringRef(value)) {}

  EventVariableType type;
  union {
    bool bool_value;
    int32_t int_value;
    float float_value;
    StringRef string_value;
  };
};

/// One value of a trigger: the index of the variable (its place in the declaration) and the value.
struct EventArg {
  uint8_t index;
  EventValue value;
};

/// Start of the data block codegen reserves for each event with variables; the values of the last event follow it.
struct EventDataHeader {
  const EventVariable *variables{nullptr};
  uint16_t present{0};  ///< bit i set: variable i has a value
  uint8_t count{0};

  uint8_t *values() { return reinterpret_cast<uint8_t *>(this + 1); }
  const uint8_t *values() const { return reinterpret_cast<const uint8_t *>(this + 1); }
};

/// The data block of one event, sized by codegen: N bytes of values.
template<size_t N> struct EventDataBlock {
  EventDataHeader header;
  uint8_t values[N];
};
static_assert(offsetof(EventDataBlock<4>, values) == sizeof(EventDataHeader), "the values must follow the header");

/// Read access to the values of the last event.
/// A variable the event did not set reads as false, 0, NAN or an empty text.
class EventData {
 public:
  explicit EventData(const EventDataHeader *data) : data_(data) {}

  /// Number of declared variables; 0 when the event declares none.
  uint8_t size() const { return this->data_ == nullptr ? 0 : this->data_->count; }
  /// Whether the last event set variable index.
  bool has(uint8_t index) const { return index < this->size() && (this->data_->present >> index) & 1; }
  /// Copy of declaration index < size(), read out of flash.
  EventVariable variable(uint8_t index) const {
    EventVariable variable;
    progmem_memcpy(&variable, &this->data_->variables[index], sizeof(variable));
    return variable;
  }

  bool get_bool(uint8_t index) const {
    const uint8_t *value = this->value_(index, EVENT_VARIABLE_TYPE_BOOL);
    return value != nullptr && *value != 0;
  }
  int32_t get_int(uint8_t index) const {
    int32_t result = 0;
    if (const uint8_t *value = this->value_(index, EVENT_VARIABLE_TYPE_INT))
      std::memcpy(&result, value, sizeof(result));
    return result;
  }
  float get_float(uint8_t index) const {
    float result = NAN;
    if (const uint8_t *value = this->value_(index, EVENT_VARIABLE_TYPE_FLOAT))
      std::memcpy(&result, value, sizeof(result));
    return result;
  }
  /// Points into the data block, so it changes with the next event.
  StringRef get_string(uint8_t index) const {
    const auto *value = reinterpret_cast<const char *>(this->value_(index, EVENT_VARIABLE_TYPE_STRING));
    return value == nullptr ? StringRef() : StringRef(value);
  }

 protected:
  /// The stored value of variable index, or nullptr when it is missing or of another type.
  const uint8_t *value_(uint8_t index, EventVariableType type) const {
    if (!this->has(index))
      return nullptr;
    const EventVariable variable = this->variable(index);
    return variable.type == type ? this->data_->values() + variable.offset : nullptr;
  }

  const EventDataHeader *data_;
};

}  // namespace esphome::event

#endif  // USE_EVENT_DATA
