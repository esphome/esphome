#include "climate_traits.h"
#include <cstdio>

namespace esphome {
namespace climate {

int8_t ClimateTraits::get_temperature_accuracy_decimals() const {
  // use printf %g to find number of digits based on temperature step
  char buf[32];
  sprintf(buf, "%.5g", this->visual_temperature_step_);
  std::string str{buf};
  size_t dot_pos = str.find('.');
  if (dot_pos == std::string::npos)
    return 0;

  return str.length() - dot_pos - 1;
}

}  // namespace climate
}  // namespace esphome
