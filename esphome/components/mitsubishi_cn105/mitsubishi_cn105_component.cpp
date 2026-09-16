#include "mitsubishi_cn105_component.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105";

void MitsubishiCN105Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Mitsubishi CN105:");
  if (this->hp_.is_telemetry_polling_enabled()) {
    ESP_LOGCONFIG(TAG, "  Telemetry polling min interval: %" PRIu32 " ms",
                  this->hp_.get_telemetry_request_min_interval());
  } else {
    ESP_LOGCONFIG(TAG, "  Telemetry polling: DISABLED");
  }
  ESP_LOGCONFIG(TAG,
                "  Update interval: %" PRIu32 " ms\n"
                "  UART: baud_rate=%" PRIu32 " data_bits=%u parity=%s stop_bits=%u",
                this->hp_.get_update_interval(), this->parent_->get_baud_rate(), this->parent_->get_data_bits(),
                LOG_STR_ARG(parity_to_str(this->parent_->get_parity())), this->parent_->get_stop_bits());
}

void MitsubishiCN105Component::setup() { this->hp_.initialize(); }

void MitsubishiCN105Component::loop() {
  if (this->hp_.update()) {
    // Encoding A only supports whole °C values and cannot represent native °F setpoints accurately.
    // See https://github.com/esphome/esphome/pull/15488#issuecomment-5268304343
    if (this->temperature_mapping_.get_use_fahrenheit() && !this->hp_.is_temperature_encoding_b()) {
      ESP_LOGE(TAG, "Unit reports encoding A, which cannot accurately convert °F setpoints; disable 'use_fahrenheit'");
      this->mark_failed();
      return;
    }
    this->notify_status_listeners_();
  }
}

void VaneCall::perform() {
  if (const auto &direction = this->vertical.get_direction(); direction.has_value()) {
    this->parent_->set_vane_mode(static_cast<MitsubishiCN105::VaneMode>(*direction));
  }
  this->parent_->publish_status();
}

}  // namespace esphome::mitsubishi_cn105
