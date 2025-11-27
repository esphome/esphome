#include "sntp_component.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
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
#ifdef USE_ESP32
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_set_sync_interval(this->get_update_interval());
  esp_sntp_init();
#else
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
#endif
  setup_servers_();
  this->servers_was_setup_ = true;

  for (size_t i = 0; i < this->servers_.size(); ++i) {
    const auto &buff = server_name_buffer(this->servers_[i]);
    if (buff != nullptr && 0 != strcmp(buff,
#ifdef USE_ESP32
                                       esp_sntp_getservername(i)
#else
                                       sntp_getservername(i)
#endif
                                           )) {
      ESP_LOGCONFIG(TAG, "Can't set server %d", i + 1);
    }
  }
#ifdef USE_ESP32
  // We can't call `stop_poller` here because it will be started after `setup` call. So defer the execution.
  this->defer([this] {
    // Stop the puller to prevent periodically update calls. IDF Handles periodic updates by itself.
    // The user still able to call `update` to trigger manual update.
    this->stop_poller();
  });
  esp_sntp_set_sync_interval(time::RealTimeClock::get_update_interval());
  esp_sntp_init();
#else
  sntp_init();
#endif
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  size_t i = 0;
  for (auto &server : this->servers_) {
    ESP_LOGCONFIG(TAG, "  Server %zu: '%s'", i++, server);
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
  RealTimeClock::dump_config();
}
void SNTPComponent::set_servers(const std::array<const char *, SNTP_SERVER_COUNT> &servers) {
  if (this->servers_was_setup_) {
    // Cleanup all the pointers to prevent use after free
    for (size_t i = 0; i < this->servers_.size(); ++i) {
      const auto &buff = this->servers_[i];
#ifdef USE_ESP32
      if (buff != nullptr && 0 == strcmp(buff, esp_sntp_getservername(i)))
        esp_sntp_setservername(i, nullptr);
#else
      if (buff != nullptr && 0 == strcmp(buff, sntp_getservername(i)))
        sntp_setservername(i, nullptr);
#endif
    }
  }

  this->servers_ = std::move(servers);

  if (this->servers_was_setup_) {
    this->setup_servers_();
  }
}
void SNTPComponent::update() {
  // force resync
#ifdef USE_ESP32
  if (esp_sntp_enabled()) {
    ESP_LOGD(TAG, "Force resync");
    sntp_restart();
#else
  // Some platforms currently cannot set the sync interval at runtime so we need
  // to do the re-sync by hand for now.
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
#endif
  }
}
void SNTPComponent::loop() {
#ifdef USE_ESP32
  // Tah sntp_get_sync_status call returns SNTP_SYNC_STATUS_COMPLETED only once when time just synched. So we don't need
  // any callbacks
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;
#else  // defined(USE_ESP32)
  if (this->has_time_)
    return;
  auto time = this->now();
  this->has_time_ = time.is_valid();
  if (!this->has_time_)
    return;
#endif

  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  this->time_sync_callback_.call();
}
#ifdef USE_ESP32
/**
Sets the update interval calling `sntp_set_sync_interval()`.

Calls `sntp_restart()` if update interval is less then it was before as ESP IDF according documentation. Without this
cal interval will be updated only after previously scheduled update.
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
#endif  // USE_ESP32
void SNTPComponent::setup_servers_() {
  for (size_t i = 0; i < this->servers_.size(); ++i) {
    const auto &buff = server_name_buffer(this->servers_[i]);
#ifdef USE_ESP32
    esp_sntp_setservername(i, buff);
    if (buff != nullptr && buff != esp_sntp_getservername(i)) {
#else
    sntp_setservername(i, buff);
    if (buff != nullptr && buff != sntp_getservername(i)) {
#endif
      ESP_LOGE(TAG, "Can't set server %d", i + 1);
    }
  }
}

}  // namespace sntp
}  // namespace esphome
