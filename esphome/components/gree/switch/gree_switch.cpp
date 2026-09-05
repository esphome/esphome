#include "gree_switch.h"
#include "esphome/core/log.h"

namespace esphome::gree {

static const char *const TAG = "gree.switch";

void GreeFeatureSwitch::setup() {
  if (this->parent_->supports_feature_state_rx()) {
    this->set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    this->publish_state(this->parent_->get_feature_state(this->feature_));
    return;
  }

  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  } else {
    this->publish_state(this->parent_->get_feature_state(this->feature_));
  }
}

void GreeFeatureSwitch::dump_config() { log_switch(TAG, "  ", this->name_, this); }

void GreeFeatureSwitch::write_state(bool state) { this->parent_->set_feature_state(this->feature_, state); }

}  // namespace esphome::gree
