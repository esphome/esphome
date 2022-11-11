#include "info_sensor.h"
#include "../jablotron/jablotron_component.h"
#include "../jablotron/string_view.h"

namespace esphome {
namespace jablotron_info {

using namespace jablotron;

void InfoSensor::register_parent(JablotronComponent &parent) { parent.register_info(this); }

void InfoSensor::set_state(StringView value) { this->publish_state(value); }

}  // namespace jablotron_info
}  // namespace esphome
