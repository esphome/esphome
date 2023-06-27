#include "climate_traits.h"

namespace esphome {
namespace climate {

int8_t ClimateTraits::get_target_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_target_temperature_step_);
}

int8_t ClimateTraits::get_current_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_current_temperature_step_);
}

}  // namespace climate
}  // namespace esphome
