#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esp_ldo.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp_ldo {

static const char *const TAG = "esp_ldo";
void EspLdo::setup() {
  esp_ldo_channel_config_t config{};
  config.chan_id = this->channel_;
  config.voltage_mv = (int) (this->voltage_ * 1000.0f);
  config.flags.adjustable = this->adjustable_;
  auto err = esp_ldo_acquire_channel(&config, &this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to acquire LDO channel %d with voltage %fV", this->channel_, this->voltage_);
    this->mark_failed(LOG_STR("Failed to acquire LDO channel"));
  } else {
    ESP_LOGD(TAG, "Acquired LDO channel %d with voltage %fV", this->channel_, this->voltage_);
  }
}
void EspLdo::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP LDO Channel %d:", this->channel_);
  ESP_LOGCONFIG(TAG, "  Voltage: %fV", this->voltage_);
  ESP_LOGCONFIG(TAG, "  Adjustable: %s", YESNO(this->adjustable_));
}

void EspLdo::adjust_voltage(float voltage) {
  if (!std::isfinite(voltage) || voltage < 0.5f || voltage > 2.7f) {
    ESP_LOGE(TAG, "Invalid voltage %fV for LDO channel %d", voltage, this->channel_);
    return;
  }
  auto erro = esp_ldo_channel_adjust_voltage(this->handle_, (int) (voltage * 1000.0f));
  if (erro != ESP_OK) {
    ESP_LOGE(TAG, "Failed to adjust LDO channel %d to voltage %fV: %s", this->channel_, voltage, esp_err_to_name(erro));
  }
}

}  // namespace esp_ldo
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32P4
