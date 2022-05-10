#include "humidifier_traits.h"
#include <cstdio>

namespace esphome {
namespace humidifier {

int8_t HumidifierTraits::get_humidity_accuracy_decimals() const {
  // use printf %g to find number of digits based on humidity step
  char buf[32];
  sprintf(buf, "%.5g", this->visual_humidity_step_);
  std::string str{buf};
  size_t dot_pos = str.find('.');
  if (dot_pos == std::string::npos)
    return 0;

  return str.length() - dot_pos - 1;
}

}  // namespace humidifier
}  // namespace esphome
