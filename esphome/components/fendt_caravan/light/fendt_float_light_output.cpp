#ifdef USE_ESP32

#include "fendt_float_ligth_output.h"

namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.FloatLightOutput");

void FendtFloatLightOutput::write_state(light::LightState *state) {
  ESP_LOGD(TAG, "write_state called, r-is_on: %s, c-is_on: %s", ONOFF(state->remote_values.is_on()),
           ONOFF(state->current_values.is_on()));
  if (this->is_on_ != state->remote_values.is_on()) {
    this->parent_->get_parent()->send_command(Commands::update_toggle(this->variable_name_));
    this->is_on_ = state->remote_values.is_on();
  } else {
    float brightness = state->remote_values.get_brightness();
    this->parent_->get_parent()->send_command(Commands::update_int(this->variable_name_, (int) (brightness * 15.0f)));
  }
}
}  // namespace esphome::fendt_caravan
#endif
