#pragma once

#include "opentherm_base.h"

namespace esphome {
namespace opentherm {
namespace message_data {

/*
 * Accessor templates to get and set values in the OpenthermData structure.
 * Each accessor defines a ValueType, a static get() and a static set() method.
 *
 * Usage example:
 *   OpenthermData data;
 *   message_data::u8_hb::set(data, 42);
 *   uint8_t value = message_data::u8_hb::get(data);
 *
 * Because these are (templated) classes they can be used in other templated classes.
 * An example can be found in OpenthermSetting in setting.h.
 */

template<uint8_t OpenthermData::*Field, uint8_t Bit> struct BitAccessor {
  using ValueType = bool;

  static_assert(Bit >= 0 && Bit < 8, "Bit index must be 0..7");

  static inline bool get(const OpenthermData &data) { return read_bit(data.*Field, Bit); }
  static inline void set(OpenthermData &data, bool value) { data.*Field = write_bit(data.*Field, Bit, value); }
};

using flag8_lb_0 = BitAccessor<&OpenthermData::valueLB, 0>;
using flag8_lb_1 = BitAccessor<&OpenthermData::valueLB, 1>;
using flag8_lb_2 = BitAccessor<&OpenthermData::valueLB, 2>;
using flag8_lb_3 = BitAccessor<&OpenthermData::valueLB, 3>;
using flag8_lb_4 = BitAccessor<&OpenthermData::valueLB, 4>;
using flag8_lb_5 = BitAccessor<&OpenthermData::valueLB, 5>;
using flag8_lb_6 = BitAccessor<&OpenthermData::valueLB, 6>;
using flag8_lb_7 = BitAccessor<&OpenthermData::valueLB, 7>;

using flag8_hb_0 = BitAccessor<&OpenthermData::valueHB, 0>;
using flag8_hb_1 = BitAccessor<&OpenthermData::valueHB, 1>;
using flag8_hb_2 = BitAccessor<&OpenthermData::valueHB, 2>;
using flag8_hb_3 = BitAccessor<&OpenthermData::valueHB, 3>;
using flag8_hb_4 = BitAccessor<&OpenthermData::valueHB, 4>;
using flag8_hb_5 = BitAccessor<&OpenthermData::valueHB, 5>;
using flag8_hb_6 = BitAccessor<&OpenthermData::valueHB, 6>;
using flag8_hb_7 = BitAccessor<&OpenthermData::valueHB, 7>;

template<uint8_t OpenthermData::*Field, typename T> struct FieldAccessor {
  using ValueType = T;

  static inline T get(const OpenthermData &data) { return static_cast<T>(data.*Field); }
  static inline void set(OpenthermData &data, T value) { data.*Field = static_cast<uint8_t>(value); }
};

using u8_lb = FieldAccessor<&OpenthermData::valueLB, uint8_t>;
using u8_hb = FieldAccessor<&OpenthermData::valueHB, uint8_t>;
using s8_lb = FieldAccessor<&OpenthermData::valueLB, int8_t>;
using s8_hb = FieldAccessor<&OpenthermData::valueHB, int8_t>;

template<uint8_t OpenthermData::*Field, typename T, T Scale> struct ScaledFieldAccessor {
  using ValueType = T;

  static inline T get(const OpenthermData &data) { return static_cast<T>(data.*Field) * Scale; }
  static inline void set(OpenthermData &data, T value) { data.*Field = static_cast<uint8_t>(value / Scale); }
};

// Used to scale RPM values
using u8_lb_60 = ScaledFieldAccessor<&OpenthermData::valueLB, uint16_t, 60>;
using u8_hb_60 = ScaledFieldAccessor<&OpenthermData::valueHB, uint16_t, 60>;

template<typename T, T (OpenthermData::*Getter)() const, void (OpenthermData::*Setter)(T)> struct MethodAccessor {
  using ValueType = T;

  static inline T get(const OpenthermData &data) { return (data.*Getter)(); }
  static inline void set(OpenthermData &data, T value) { (data.*Setter)(value); }
};

using u16 = MethodAccessor<uint16_t, &OpenthermData::u16, &OpenthermData::u16>;
using s16 = MethodAccessor<int16_t, &OpenthermData::s16, &OpenthermData::s16>;
using f88 = MethodAccessor<float, &OpenthermData::f88, &OpenthermData::f88>;

}  // namespace message_data
}  // namespace opentherm
}  // namespace esphome
