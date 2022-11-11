#include "section_flag_sensor.h"
#include "../jablotron/jablotron_component.h"

namespace esphome {
namespace jablotron_section_flag {

void SectionFlagSensor::register_parent(jablotron::JablotronComponent &parent) { parent.register_section_flag(this); }

void SectionFlagSensor::set_state(bool value) {
  if (this->last_value_ != value) {
    this->publish_state(value);
    this->last_value_ = value;
  }
}

}  // namespace jablotron_section_flag
}  // namespace esphome
