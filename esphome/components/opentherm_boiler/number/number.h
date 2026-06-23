#pragma once

#include "../boiler.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class BoilerNumber : public number::Number, public RequestProcessor {
 public:
  BoilerNumber() = default;

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::READ_DATA && this->has_state()) {
      T::set(data, this->value_);
      return true;
    } else if (data.type == MessageType::WRITE_DATA) {
      this->value_ = T::get(data);
      this->publish_state(this->value_);
      return true;
    }
    return false;
  }

  const char *get_type_name() const override { return "number"; }

 protected:
  float value_{NAN};

  void control(float value) override {
    this->publish_state(value);
    this->value_ = value;
  }
};

}  // namespace opentherm_boiler
}  // namespace esphome
