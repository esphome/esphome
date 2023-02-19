#include "controller.h"
#include "esphome/core/log.h"

namespace esphome {

void Controller::setup_controller(bool include_internal) { include_internal_ = include_internal; }

}  // namespace esphome
