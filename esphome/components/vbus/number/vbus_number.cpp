#include "vbus_number.h"

#ifdef USE_VBUS_CS_PLUS_ENTITIES
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.number");

void VBusSettingNumber::dump_setting_config() {
  LOG_NUMBER("", "VBus Setting", this);
  ESP_LOGCONFIG(TAG, "  Setting ID: %u", this->index_);
}

void VBusSettingNumber::on_setting_snapshot() {
  if (!this->controller_->has_fresh_snapshot() && this->controller_->has_recent_snapshot())
    return;
  float minimum, maximum, step;
  const bool available = this->controller_->setting_range(this->index_, minimum, maximum, step);
  if (available) {
    if (this->traits.get_min_value() != minimum || this->traits.get_max_value() != maximum ||
        this->traits.get_step() != step)
      this->controller_->note_metadata_change();
    this->traits.set_min_value(minimum);
    this->traits.set_max_value(maximum);
    this->traits.set_step(step);
  }
  const float value = available ? this->controller_->setting_value(this->index_) : NAN;
  if (!std::isfinite(value)) {
    if (this->has_state()) {
      this->set_has_state(false);
#ifdef USE_CONTROLLER_REGISTRY
      ControllerRegistry::notify_number_update(this);
#endif
    }
  } else if (!this->has_state() || this->state != value) {
    this->publish_state(value);
  }
}

void VBusSettingNumber::control(float value) {
  if (!this->controller_->request_setting(this->index_, value)) {
    ESP_LOGW(TAG, "Setting %u request rejected; retaining confirmed state", this->index_);
  }
}

}  // namespace esphome::vbus
#endif
