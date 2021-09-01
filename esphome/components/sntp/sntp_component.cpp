#include "sntp_component.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include "lwip/apps/sntp.h"
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include "sntp.h"
#endif

// Yes, the server names are leaked, but that's fine.
#ifdef CLANG_TIDY
#define strdup(x) (const_cast<char *>(x))
#endif

namespace esphome {
namespace sntp {

static const char *const TAG = "sntp";

void SNTPComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SNTP...");
#ifdef ARDUINO_ARCH_ESP32
  if (sntp_enabled()) {
    sntp_stop();
  }
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
#endif
#ifdef ARDUINO_ARCH_ESP8266
  sntp_stop();
#endif

  sntp_setservername(0, strdup(this->server_1_.c_str()));
  if (!this->server_2_.empty()) {
    sntp_setservername(1, strdup(this->server_2_.c_str()));
  }
  if (!this->server_3_.empty()) {
    sntp_setservername(2, strdup(this->server_3_.c_str()));
  }

  sntp_init();
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  ESP_LOGCONFIG(TAG, "  Server 1: '%s'", this->server_1_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 2: '%s'", this->server_2_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 3: '%s'", this->server_3_.c_str());
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}
void SNTPComponent::update() {}
void SNTPComponent::loop() {
  if (this->has_time_)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;

  ESP_LOGD(TAG, "Synchronized time: %d-%d-%d %d:%d:%d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  this->time_sync_callback_.call();
  this->has_time_ = true;
}

}  // namespace sntp
}  // namespace esphome
