#include <cinttypes>
#include "mitsubishi_cn105_climate.h"
#include "esphome/core/log.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.climate";

void MitsubishiCN105Climate::dump_config() {
  LOG_CLIMATE("", "Mitsubishi CN105 Climate", this);
  ESP_LOGCONFIG(TAG,
                "  Update interval: %" PRIu32 " ms\n"
                "  UART: baud_rate=%" PRIu32 " data_bits=%u parity=%s stop_bits=%u",
                this->hp_.get_update_interval(), this->parent_->get_baud_rate(), this->parent_->get_data_bits(),
                LOG_STR_ARG(parity_to_str(this->parent_->get_parity())), this->parent_->get_stop_bits());
}

void MitsubishiCN105Climate::setup() { this->hp_.initialize(); }

void MitsubishiCN105Climate::loop() {
  if (this->hp_.update()) {
    this->apply_values_();
  }
}

climate::ClimateTraits MitsubishiCN105Climate::traits() {
  climate::ClimateTraits traits;

  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(31.0f);
  traits.set_visual_temperature_step(1.0f);
  traits.set_visual_current_temperature_step(0.5f);

  return traits;
}

void MitsubishiCN105Climate::control(const climate::ClimateCall &call) {}

void MitsubishiCN105Climate::apply_values_() {
  const auto &status = this->hp_.status();

  this->target_temperature = status.target_temperature;
  this->current_temperature = status.room_temperature;

  this->publish_state();
}

}  // namespace esphome::mitsubishi_cn105
