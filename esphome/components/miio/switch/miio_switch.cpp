#include "esphome/core/log.h"
#include "miio_switch.h"

namespace esphome {
namespace miio {

static const char *const TAG = "miio.switch";

void MiioSwitch::setup() {
  this->parent_->register_listener(this->switch_id_, MiioPropertyType::BOOLEAN,
                                   [this](const MiioPropertyValue &value) { this->publish_state(value.value_bool); });
}

void MiioSwitch::write_state(bool state) {
  this->parent_->set_property(this->switch_id_, state ? "true" : "false");
  this->publish_state(state);
}

void MiioSwitch::dump_config() {
  LOG_SWITCH("", "MIIO Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch has cluster ID: %u", std::get<0>(this->switch_id_));
  ESP_LOGCONFIG(TAG, "  Switch has key ID: %u", std::get<1>(this->switch_id_));
}

}  // namespace miio
}  // namespace esphome
