#include "gree_switch.h"
#include "esphome/core/log.h"

namespace esphome::gree {

#ifdef ESPHOME_LOG_TAG
ESPHOME_LOG_TAG(TAG, "gree.switch");
#elif defined(USE_ESP8266)
static const char TAG[] PROGMEM = "gree.switch";
#else
static constexpr const char *const TAG = "gree.switch";
#endif

void GreeFeatureSwitch::setup() {
  if (this->parent_->supports_feature_state_rx()) {
    if (this->restore_mode != switch_::SWITCH_RESTORE_DISABLED) {
      ESP_LOGD(TAG, "GREE switch %s: disabling restore_mode; state is synchronized from IR RX", this->name_);
      this->set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    }
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
