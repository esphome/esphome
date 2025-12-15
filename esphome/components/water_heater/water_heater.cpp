#include "water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace water_heater {

static const char *const TAG = "water_heater";

void WaterHeater::setup() {}

void WaterHeater::publish_state() {
  ESP_LOGD(TAG, "Publishing state: Temp=%.1f, Target=%.1f, Mode=%d", this->current_temperature,
           this->target_temperature, (int) this->mode);
}

void TemplateWaterHeater::dump_config() {
  ESP_LOGCONFIG(TAG, "Template Water Heater '%s'", this->get_name().c_str());
  LOG_COMPONENT_SETUP(TAG, this);
}

void TemplateWaterHeater::control(const WaterHeaterCall &call) {
  if (call.mode.has_value()) {
    this->mode = *call.mode;
    this->mode_trigger_->trigger(this->mode);
  }
  if (call.target_temperature.has_value()) {
    this->target_temperature = *call.target_temperature;
    this->temperature_trigger_->trigger(this->target_temperature);
  }
  this->publish_state();
}

}  // namespace water_heater
}  // namespace esphome