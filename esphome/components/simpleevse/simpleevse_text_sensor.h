#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "simplesevse.h"

namespace esphome {
namespace simpleevse {

class SimpleEvseTextSensors : public UpdateListener {
 public:
  explicit SimpleEvseTextSensors(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }

  /* setter methods for sensor configuration */
  void set_vehicle_state_sensor(text_sensor::TextSensor *vehicle_state_sensor) {
    vehicle_state_sensor_ = vehicle_state_sensor;
  }
  void set_evse_state_sensor(text_sensor::TextSensor *evse_state_sensor) { evse_state_sensor_ = evse_state_sensor; }

  void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) override;

 protected:
  SimpleEvseComponent *const parent_;

  text_sensor::TextSensor *vehicle_state_sensor_{nullptr};
  text_sensor::TextSensor *evse_state_sensor_{nullptr};
};

}  // namespace simpleevse
}  // namespace esphome
