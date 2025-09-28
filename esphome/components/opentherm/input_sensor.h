#pragma once

#include "hub.h"

namespace esphome {
namespace opentherm {

/*
 * An input sensor is a message with a value derived from a Sensor, likely from a different platform.
 * This could be used for example to set the boiler temperature setpoint based on a temperature sensor reading.
 *
 * Usage example:
 *   auto controller_id = new opentherm::OpenthermInputSensor<opentherm::message_data::f88>(opentherm::ROOM_TEMP);
 *   t_room->set_sensor(some_room_temperature_sensor);
 *
 *   auto data = opentherm::OpenthermData();
 *   controller_id->prepare_output(data);
 *   // data now contains f88 = some_room_temperature_sensor->state
 */
template<typename T> class OpenthermInputSensor : public MessageProcessor {
 public:
  inline void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  void prepare_data_out(OpenthermData &data) const override {
    if (this->sensor_->has_state()) {
      data.type = MessageType::WRITE_DATA;
      T::set(data, this->sensor_->get_state());
    } else {
      data.type = MessageType::INVALID_DATA;
    }
  }

  const char *get_type_name() const override { return "input_sensor"; }

 protected:
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace opentherm
}  // namespace esphome
