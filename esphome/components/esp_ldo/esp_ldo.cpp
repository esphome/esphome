#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esp_ldo.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::esp_ldo {

static const char *const TAG = "esp_ldo";
void EspLdo::setup() {
  esp_ldo_channel_config_t config{};
  config.chan_id = this->channel_;
  config.voltage_mv = this->voltage_mv_;
  config.flags.adjustable = this->adjustable_;
  auto err = esp_ldo_acquire_channel(&config, &this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to acquire LDO channel %d with voltage %dmV", this->channel_, this->voltage_mv_);
    this->mark_failed(LOG_STR("Failed to acquire LDO channel"));
  } else {
    ESP_LOGD(TAG, "Acquired LDO channel %d with voltage %dmV", this->channel_, this->voltage_mv_);
  }
}
void EspLdo::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP LDO Channel %d:\n"
                "  Voltage: %dmV\n"
                "  Adjustable: %s",
                this->channel_, this->voltage_mv_, YESNO(this->adjustable_));
}

void EspLdo::adjust_voltage(float voltage) {
  if (!std::isfinite(voltage) || voltage < 0.5f || voltage > 2.7f) {
    ESP_LOGE(TAG, "Invalid voltage %fV for LDO channel %d (must be 0.5V-2.7V)", voltage, this->channel_);
    return;
  }
  int voltage_mv = (int) roundf(voltage * 1000.0f);
  auto err = esp_ldo_channel_adjust_voltage(this->handle_, voltage_mv);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to adjust LDO channel %d to voltage %dmV: %s", this->channel_, voltage_mv,
             esp_err_to_name(err));
  }
}

}  // namespace esphome::esp_ldo

#endif  // USE_ESP32_VARIANT_ESP32P4
