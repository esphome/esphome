#include "remote.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote {

static const char *const TAG = "remote";

void Remote::turn_on() {
  ESP_LOGD(TAG, "'%s' Turning ON.", this->get_name().c_str());
  this->write_state(true);
  this->state_callback_.call(true);
}
void Remote::turn_off() {
  ESP_LOGD(TAG, "'%s' Turning OFF.", this->get_name().c_str());
  this->write_state(false);
  this->state_callback_.call(false);
}

}  // namespace remote

}  // namespace esphome
