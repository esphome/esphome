#pragma once

#include "boiler.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageType;

template<typename T> class LambdaValue : public RequestProcessor {
  using value_fn_t = std::function<optional<typename T::ValueType>(OpenthermData &)>;

 public:
  LambdaValue(value_fn_t handler) : handler_(handler) {}

  bool handle_request(OpenthermData &data) override {
    if (data.type == MessageType::READ_DATA) {
      optional<typename T::ValueType> value = this->handler_(data);

      if (value.has_value()) {
        T::set(data, value.value());
        return true;
      }
    }

    return false;
  }

  const char *get_type_name() const override { return "lambda_value"; }

 protected:
  value_fn_t handler_;
};

}  // namespace opentherm_boiler
}  // namespace esphome
