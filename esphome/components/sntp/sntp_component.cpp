#include "sntp_component.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_sntp.h"
#elif USE_ESP8266
#include "sntp.h"
#else
#include "lwip/apps/sntp.h"
#endif

namespace esphome {
namespace sntp {

static const char *const TAG = "sntp";

void SNTPComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SNTP...");
#if defined(USE_ESP_IDF)
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  size_t i = 0;
  for (auto &server : this->servers_) {
    esp_sntp_setservername(i++, server.c_str());
  }
  esp_sntp_set_sync_interval(this->get_update_interval());
  esp_sntp_init();
#else
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);

  size_t i = 0;
  for (auto &server : this->servers_) {
    sntp_setservername(i++, server.c_str());
  }
  sntp_init();
#endif
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  size_t i = 0;
  for (auto &server : this->servers_) {
    ESP_LOGCONFIG(TAG, "  Server %zu: '%s'", i++, server.c_str());
  }
}
void SNTPComponent::update() {
#if !defined(USE_ESP_IDF)
  // force resync
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
  }
#endif
}
void SNTPComponent::loop() {
  if (this->has_time_)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;

  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  this->time_sync_callback_.call();
  this->has_time_ = true;
}

}  // namespace sntp
}  // namespace esphome
