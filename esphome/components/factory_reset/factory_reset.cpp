#include "factory_reset.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

#if !defined(USE_RP2040) && !defined(USE_HOST)

namespace esphome {
namespace factory_reset {

static const char *const TAG = "factory_reset";
static const uint32_t POWER_CYCLES_KEY = 0xFA5C0DE;

static bool was_power_cycled() {
#ifdef USE_ESP32
  return esp_reset_reason() == ESP_RST_POWERON;
#endif
#ifdef USE_ESP8266
  auto reset_reason = EspClass::getResetReason();
  return strcasecmp(reset_reason.c_str(), "power On") == 0 || strcasecmp(reset_reason.c_str(), "external system") == 0;
#endif
#ifdef USE_LIBRETINY
  auto reason = lt_get_reboot_reason();
  return reason == REBOOT_REASON_POWER || reason == REBOOT_REASON_HARDWARE;
#endif
}

void FactoryResetComponent::dump_config() {
  uint8_t count = 0;
  this->flash_.load(&count);
  ESP_LOGCONFIG(TAG, "Factory Reset by Reset:");
  ESP_LOGCONFIG(TAG,
                "  Max interval between resets %" PRIu32 " seconds\n"
                "  Current count: %u\n"
                "  Factory reset after %u resets",
                this->max_interval_ / 1000, count, this->required_count_);
}

void FactoryResetComponent::save_(uint8_t count) {
  this->flash_.save(&count);
  global_preferences->sync();
  this->defer([count, this] { this->increment_callback_.call(count, this->required_count_); });
}

void FactoryResetComponent::setup() {
  this->flash_ = global_preferences->make_preference<uint8_t>(POWER_CYCLES_KEY, true);
  if (was_power_cycled()) {
    uint8_t count = 0;
    this->flash_.load(&count);
    // this is a power on reset or external system reset
    count++;
    if (count == this->required_count_) {
      ESP_LOGW(TAG, "Reset count reached, factory resetting");
      global_preferences->reset();
      // delay to allow log to be sent
      delay(100);         // NOLINT
      App.safe_reboot();  // should not return
    }
    this->save_(count);
    ESP_LOGD(TAG, "Power on reset detected, incremented count to %u", count);
    this->set_timeout(this->max_interval_, [this]() {
      ESP_LOGD(TAG, "No reset in the last %" PRIu32 " seconds, resetting count", this->max_interval_ / 1000);
      this->save_(0);  // reset count
    });
  } else {
    this->save_(0);  // reset count if not a power cycle
  }
}

}  // namespace factory_reset
}  // namespace esphome

#endif  // !defined(USE_RP2040) && !defined(USE_HOST)
