#include "update_entity.h"

namespace esphome {
namespace update {

void UpdateEntity::publish_state() {
  this->has_state_ = true;
  this->state_callback_.call();
}

}  // namespace update
}  // namespace esphome
