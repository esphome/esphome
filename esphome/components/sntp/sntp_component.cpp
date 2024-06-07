#include "sntp_component.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include "lwip/apps/sntp.h"
#ifdef USE_ESP_IDF
#include "esp_sntp.h"
#endif
#endif
#ifdef USE_ESP8266
#include "sntp.h"
#endif
#ifdef USE_RP2040
#include "lwip/apps/sntp.h"
#endif

namespace esphome {
namespace sntp {

static const char *const TAG = "sntp";
static const std::string FORCE_UPDATE_SCHEDULE = "force_update_schedule";

const char *server_name_buffer(const std::string &server) { return server.empty() ? nullptr : server.c_str(); }

#ifdef USE_ESP_IDF
static time_t sync_time_to_report_ = 0;
#endif

void SNTPComponent::setup() {
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG, "Setting up SNTP...");
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  if (sntp_enabled()) {
    sntp_stop();
  }
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
#endif
#ifdef USE_ESP8266
  sntp_stop();
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
  this->stop_poller();
  sntp_set_time_sync_notification_cb([](struct timeval *tv) { sync_time_to_report_ = tv->tv_sec; });
#endif  // USE_ESP_IDF

  sntp_init();
#endif  // USE_HOST
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  ESP_LOGCONFIG(TAG, "  Server 1: '%s'", this->server_1_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 2: '%s'", this->server_2_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 3: '%s'", this->server_3_.c_str());
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}
void set_servers(const std::string &server_1, const std::string &server_2, const std::string &server_3) {
#if !defined(USE_HOST)
  if (servers_was_setup_) {
    for (uint8_t i = 0; i < 3; ++i) {
      const auto &buff = this->servers_[i].empty() ? nullptr : this->servers_[i].c_str();
      if (buff != nullptr && buff == sntp_getservername(i))
        sntp_setservername(i, nullptr);
      max_setup_server = max(max_setup_server, i);
    }
  }
#endif
  this->servers_ = {server_1, server_2, server_3};
#if !defined(USE_HOST)
  if (this->servers_was_setup_) {
    setup_servers_();
  }
#endif
}
void SNTPComponent::update() {
#if !defined(USE_HOST)
  // force resync
  if (sntp_enabled()) {
#if defined(USE_ESP_IDF)
    sntp_restart();
#else
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
#endif
  }
#endif
}
void SNTPComponent::loop() {
#ifdef USE_ESP_IDF
  if (sync_time_to_report_ != 0) {
    this->cancel_timeout(FORCE_UPDATE_SCHEDULE);
    time_t time_to_report = 0;
    std::swap(sync_time_to_report_, time_to_report);
    const ESPTime time = ESPTime::from_epoch_local(time_to_report);
    ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month,
             time.hour, time.minute, time.second);
  }
#else
  if (this->has_time_)
    return;

  auto time = this->now();
  if (!time.is_valid())
    return;

  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
#endif
  this->time_sync_callback_.call();
  this->has_time_ = true;
}
#ifdef USE_ESP_IDF
void SNTPComponent::set_update_interval(uint32_t update_interval) {
  time::RealTimeClock::set_update_interval(update_interval);
  const auto previous_sync_interval = sntp_get_sync_interval();

  sntp_set_sync_interval(update_interval);
  const auto new_sync_interval = sntp_get_sync_interval();

  if (previous_sync_interval > new_sync_interval) {
    this->set_timeout(FORCE_UPDATE_SCHEDULE, new_sync_interval, [] { sntp_restart(); });
  }
}
uint32_t SNTPComponent::get_update_interval() const { return sntp_get_sync_interval(); }
#endif
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
