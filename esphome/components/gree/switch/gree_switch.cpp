#include "gree_switch.h"
#include "esphome/core/log.h"

namespace esphome::gree {

ESPHOME_LOG_TAG(TAG, "gree.switch");

void GreeFeatureSwitch::setup() {
  this->restore();
  // A disabled restore mode keeps the parent's state, which models that receive IR keep in sync.
  if (!this->has_state())
    this->publish_state(this->parent_->get_feature_state(this->feature_));
}

void GreeFeatureSwitch::dump_config() { log_switch(TAG, "  ", this->name_, this); }

void GreeFeatureSwitch::write_state(bool state) { this->parent_->set_feature_state(this->feature_, state); }

}  // namespace esphome::gree
