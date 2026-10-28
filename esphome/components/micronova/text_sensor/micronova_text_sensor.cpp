#include "micronova_text_sensor.h"
#include "esphome/core/progmem.h"

namespace esphome::micronova {

ESPHOME_LOG_TAG(TAG, "micronova.text_sensor");

PROGMEM_STRING_TABLE(StoveStateStrings, "Off", "Start", "Pellets loading", "Ignition", "Working", "Brazier Cleaning",
                     "Final Cleaning", "Standby", "No pellets alarm", "No ignition alarm", "Undefined alarm");

void MicroNovaTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Micronova text sensor", this);
  this->dump_base_config();
}

void MicroNovaTextSensor::process_value_from_stove(int value_from_stove) {
  if (value_from_stove == -1) {
    this->publish_state("unknown");
    return;
  }

  // Values past the table read as "Undefined alarm"
  this->publish_state(
      StoveStateStrings::get_progmem_str(static_cast<uint8_t>(value_from_stove), StoveStateStrings::LAST_INDEX));
}

}  // namespace esphome::micronova
