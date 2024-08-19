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

const char *server_name_buffer(const std::string &server) { return server.empty() ? nullptr : server.c_str(); }

void SNTPComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SNTP...");
#if defined(USE_ESP_IDF)
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
#else
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
#endif

  setup_servers_();
  this->servers_was_setup_ = true;

  for (uint8_t i = 0; i < 3; ++i) {
    const auto &buff = server_name_buffer(servers_[i]);
    if (buff != nullptr && buff != sntp_getservername(i)) {
      ESP_LOGCONFIG(TAG, "Can't set server %d", i + 1);
    }
  }
#ifdef USE_ESP_IDF
  // Stop the puller to prevent periodically update calls. IDF Handles periodic updates by itself.
  // The user still able to call `update` to trigger manual update.
  this->stop_poller();
  esp_sntp_set_sync_interval(this->get_update_interval());
#endif

  sntp_init();
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  for (uint8_t i = 0; i < 3; ++i) {
    ESP_LOGCONFIG(TAG, "  Server %d: '%s'", i + 1, this->servers_[i].c_str());
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}
void SNTPComponent::set_servers(const std::string &server_1, const std::string &server_2, const std::string &server_3) {
  if (this->servers_was_setup_) {
    for (uint8_t i = 0; i < 3; ++i) {
      const auto &buff = this->servers_[i].empty() ? nullptr : this->servers_[i].c_str();
      if (buff != nullptr && buff == sntp_getservername(i))
        sntp_setservername(i, nullptr);
    }
  }

  this->servers_[0] = server_1;
  this->servers_[1] = server_2;
  this->servers_[2] = server_3;

  if (this->servers_was_setup_) {
    this->setup_servers_();
  }
}
void SNTPComponent::update() {
  // force resync
#if defined(USE_ESP_IDF)
  if (esp_sntp_enabled()) {
    ESP_LOGD(TAG, "Force resync");
    sntp_restart();
#else
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
#endif
  }
}
void SNTPComponent::loop() {
#if defined(USE_ESP_IDF)
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;
#else   // defined(USE_ESP_IDF)
  if (this->has_time_)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;

  this->has_time_ = true;
#endif  // defined(USE_ESP_IDF)
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  this->time_sync_callback_.call();
}
#if defined(USE_ESP_IDF)
/**
Sets the update interval calling `sntp_set_sync_interval()`.

Calls `sntp_restart()` if update interval is less then it was before as ESP IDF according documentation. Without this cal interval will be updated only after previously scheduled update.
*/
void SNTPComponent::set_update_interval(uint32_t update_interval) {
  const auto previous_sync_interval = sntp_get_sync_interval();

  sntp_set_sync_interval(update_interval);
  const auto new_sync_interval = sntp_get_sync_interval();
  time::RealTimeClock::set_update_interval(new_sync_interval);

  if (previous_sync_interval > new_sync_interval) {
    sntp_restart();
  }
}
/**
Redirects the call to `sntp_get_sync_interval()` so it's only one source of truth.
*/
uint32_t SNTPComponent::get_update_interval() const { return sntp_get_sync_interval(); }
#endif  // defined(USE_ESP_IDF)
void SNTPComponent::setup_servers_() {
  for (uint8_t i = 0; i < 3; ++i) {
    const auto &buff = server_name_buffer(this->servers_[i]);
    sntp_setservername(i, buff);
    if (buff != nullptr && buff != sntp_getservername(i)) {
      ESP_LOGE(TAG, "Can't set server %d", i + 1);
    }
  }
}

}  // namespace sntp
}  // namespace esphome
