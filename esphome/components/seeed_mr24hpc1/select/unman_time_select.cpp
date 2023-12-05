#include "unman_time_select.h"

namespace esphome {
namespace mr24hpc1 {

void UnmanTimeSelect::control(const std::string &value) {
  this->parent_->set_unman_time(value);
}

}  // namespace mr24hpc1
}  // namespace esphome
