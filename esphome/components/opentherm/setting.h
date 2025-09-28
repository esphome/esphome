#pragma once

#include "hub.h"

namespace esphome {
namespace opentherm {

/*
 * A setting is a message with a fixed value.
 *
 * Usage example:
 *   auto controller_id = new opentherm::OpenthermSetting<opentherm::message_data::u8_lb>(opentherm::CONTROLLER_CONFIG);
 *   controller_id->set_value(123);
 *
 *   auto data = opentherm::OpenthermData();
 *   controller_id->prepare_output(data);
 *   // data now contains valueLB = 123
 */
template<typename T> class OpenthermSetting : public MessageProcessor {
 public:
  inline void set_value(typename T::ValueType value) { this->value_ = value; }

  void prepare_data_out(OpenthermData &data) const override {
    data.type = MessageType::WRITE_DATA;
    T::set(data, this->value_);
  }

  const char *get_type_name() const override { return "setting"; }

 protected:
  typename T::ValueType value_{};
};

}  // namespace opentherm
}  // namespace esphome
