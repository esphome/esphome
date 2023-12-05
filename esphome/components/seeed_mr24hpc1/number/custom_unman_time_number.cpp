#include "custom_unman_time_number.h"

namespace esphome {
namespace mr24hpc1 {

void CustomUnmanTimeNumber::control(float value) {
  this->parent_->set_custom_unman_time(value);
}

}  // namespace mr24hpc1
}  // namespace esphome
