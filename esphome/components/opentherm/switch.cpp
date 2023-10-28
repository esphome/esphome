#include "switch.h"

namespace esphome {
namespace opentherm {

void OpenthermSwitch::write_state(bool state) {
    this->publish_state(state);
}

void OpenthermSwitch::setup() {
    auto restored = this->get_initial_state_with_restore_mode();
    bool state = false;
    if (!restored.has_value())
    {
      ESP_LOGD("opentherm.switch", "Couldn't restore state for OpenTherm switch '%s'", this->get_name().c_str());
    }
    else
    {
      ESP_LOGD("opentherm.switch", "Restored state for OpenTherm switch '%s': %d", this->get_name().c_str(), restored.value());
      state = restored.value();
    }
    this->write_state(state);
}

void OpenthermSwitch::dump_config() {
    esphome::switch_::log_switch("opentherm.switch", "", "OpenTherm Switch", this);
    ESP_LOGCONFIG("opentherm.switch", "  Current state: %d", this->state);
}

} // namespace opentherm
} // namespace esphome
