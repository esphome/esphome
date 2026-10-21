#include "max170xx.h"
#include "esphome/core/log.h"

namespace esphome::max170xx {

ESPHOME_LOG_TAG(TAG, "max170xx");

template<bool MAX17048_FAMILY> void MAX170xxComponent<MAX17048_FAMILY>::setup() {
  // A silent bus is retried from update(): some boards gate the I2C rail until after setup.
  // An early on_boot may already have configured the gauge and put it to sleep, so this must not configure again.
  this->ready_for_update_();
}

template<bool MAX17048_FAMILY> bool MAX170xxComponent<MAX17048_FAMILY>::ready_for_update_() {
  if (this->configured_ || this->configure_())
    return true;
  // A wrong CONFIG value has already failed the component with its own reason
  if (!this->is_failed())
    this->status_set_warning(LOG_STR("Gauge did not answer"));
  return false;
}

template<bool MAX17048_FAMILY> bool MAX170xxComponent<MAX17048_FAMILY>::configure_() {
  uint16_t config;
  if (!this->read_byte_16(REG_CONFIG, &config))
    return false;

  const bool was_asleep = (config & CONFIG_SLEEP) != 0;
  if ((config & CONFIG_SAFE_MASK) != CONFIG_POWER_UP_DEFAULT) {
    ESP_LOGE(TAG, "CONFIG reads 0x%04X, expected 0x%04X", config & CONFIG_SAFE_MASK, CONFIG_POWER_UP_DEFAULT);
    this->mark_failed(LOG_STR("Device not recognised"));
    return false;
  }

  uint16_t version;
  if (this->read_byte_16(REG_VERSION, &version)) {
    ESP_LOGCONFIG(TAG, "IC version: 0x%04X", version);
  }

  // Write back the power-up value to clear a stale sleep bit; a failed write is retried like a silent bus
  if (!this->write_byte_16(REG_CONFIG, CONFIG_POWER_UP_DEFAULT))
    return false;

  this->configured_ = true;
  // Waking from sleep raises a false low-battery alert; a real one at an ordinary boot is left alone
  if (was_asleep)
    this->set_timeout(WAKE_ALERT_CLEAR_TIMEOUT_ID, WAKE_ALERT_CLEAR_DELAY_MS, [this]() { this->clear_wake_alert_(); });
  return true;
}

template<bool MAX17048_FAMILY> void MAX170xxComponent<MAX17048_FAMILY>::clear_wake_alert_() {
  uint16_t config;
  if (!this->read_byte_16(REG_CONFIG, &config) || !this->write_byte_16(REG_CONFIG, config & ~CONFIG_ALRT)) {
    this->status_set_warning(LOG_STR("Wake alert clear failed"));
    return;
  }
  if constexpr (MAX17048_FAMILY) {
    uint16_t status;
    if (!this->read_byte_16(REG_STATUS, &status) || !this->write_byte_16(REG_STATUS, status & ~STATUS_HD))
      this->status_set_warning(LOG_STR("Wake alert clear failed"));
  }
}

template<bool MAX17048_FAMILY> void MAX170xxComponent<MAX17048_FAMILY>::sleep_mode() {
  if (this->is_failed())
    return;
  // The deferred configuration clears the sleep bit, so it has to run before the gauge is put to sleep
  if (!this->ready_for_update_())
    return;

  // A clear still pending would otherwise run against a sleeping gauge
  if (this->cancel_timeout(WAKE_ALERT_CLEAR_TIMEOUT_ID))
    this->clear_wake_alert_();

  if constexpr (MAX17048_FAMILY) {
    if (!this->write_byte_16(REG_MODE, MODE_EN_SLEEP)) {
      this->status_set_warning(LOG_STR("Unable to enable sleep mode in the mode register"));
      return;
    }
  }

  if (!this->write_byte_16(REG_CONFIG, CONFIG_POWER_UP_DEFAULT | CONFIG_SLEEP))
    this->status_set_warning(LOG_STR("Unable to write the sleep bit to config register"));
}

template class MAX170xxComponent<false>;
template class MAX170xxComponent<true>;

}  // namespace esphome::max170xx
