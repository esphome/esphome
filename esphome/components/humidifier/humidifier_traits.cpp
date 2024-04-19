#include "humidifier_traits.h"

namespace esphome {
namespace humidifier {

int8_t HumidifierTraits::get_target_humidity_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_target_humidity_step_);
}

int8_t HumidifierTraits::get_current_humidity_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_current_humidity_step_);
}

}  // namespace humidifier
}  // namespace esphome