#pragma once

#include "boiler.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class ConstantValue : public RequestProcessor {
 public:
  ConstantValue(typename T::ValueType value) : value_(value) {}

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::READ_DATA) {
      T::set(data, this->value_);
      return true;
    }

    return false;
  }

  const char *get_type_name() const override { return "constant_value"; }

 protected:
  typename T::ValueType value_;
};

}  // namespace opentherm_boiler
}  // namespace esphome
