#include "esphome/core/log.h"
#include "number_traits.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

void NumberTraits::set_unit_of_measurement(const std::string &unit_of_measurement) {
  this->unit_of_measurement_ = unit_of_measurement;
}

std::string NumberTraits::get_unit_of_measurement() {
  if (this->unit_of_measurement_.has_value())
    return *this->unit_of_measurement_;
  return "";
}

void NumberTraits::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }

std::string NumberTraits::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return "";
}

}  // namespace number
}  // namespace esphome
