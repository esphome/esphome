#include "section_sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/jablotron/string_view.h"
#include "esphome/components/jablotron/jablotron_component.h"

namespace esphome {
namespace jablotron_section {

using namespace jablotron;

static const char *const TAG = "jablotron_section";

void SectionSensor::register_parent(JablotronComponent &parent) { parent.register_section(this); }

void SectionSensor::set_state(StringView value) {
  if (value != this->last_value_) {
    this->last_value_ = std::string{value};
    this->publish_state(this->last_value_);
  }
}

}  // namespace jablotron_section
}  // namespace esphome
