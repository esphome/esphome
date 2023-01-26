#include "climate_traits.h"

namespace esphome {
namespace climate {

int8_t ClimateTraits::get_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_temperature_step_);
}

}  // namespace climate
}  // namespace esphome
