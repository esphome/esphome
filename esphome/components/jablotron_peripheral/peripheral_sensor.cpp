#include "peripheral_sensor.h"
#include "../jablotron/jablotron_component.h"

namespace esphome {
namespace jablotron_peripheral {

using namespace jablotron;

void PeripheralSensor::register_parent(JablotronComponent &parent) { parent.register_peripheral(this); }

void PeripheralSensor::set_state(bool value) {
  if (this->last_value_ != value) {
    this->publish_state(value);
    this->last_value_ = value;
  }
}

}  // namespace jablotron_peripheral
}  // namespace esphome
