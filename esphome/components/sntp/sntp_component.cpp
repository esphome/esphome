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

// Yes, the server names are leaked, but that's fine.
#ifdef CLANG_TIDY
#define strdup(x) (const_cast<char *>(x))
#endif

namespace esphome {
namespace sntp {

static const char *const TAG = "sntp";

std::vector<std::pair<optional<struct timeval>, sntp_sync_status_t>> callback_args_;

static std::function<void(struct timeval *tv)> g_sync_callback = nullptr;

void sntp_sync_time_cb(struct timeval *tv) { g_sync_callback(tv); }

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

  sntp_setservername(0, strdup(this->server_1_.c_str()));
  if (!this->server_2_.empty()) {
    sntp_setservername(1, strdup(this->server_2_.c_str()));
  }
  if (!this->server_3_.empty()) {
    sntp_setservername(2, strdup(this->server_3_.c_str()));
  }
#ifdef USE_ESP_IDF
  sntp_set_sync_interval(this->get_update_interval());
#endif

  g_sync_callback = [this](struct timeval *tv) {
    this->callback_args_.push_back({tv ? *tv : {}, sntp_get_sync_status()});
  };
  ESP_LOGD(TAG, "Set notification callback");
  sntp_set_time_sync_notification_cb(sntp_sync_time_cb);

  sntp_init();
#endif
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  ESP_LOGCONFIG(TAG, "  Server 1: '%s'", this->server_1_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 2: '%s'", this->server_2_.c_str());
  ESP_LOGCONFIG(TAG, "  Server 3: '%s'", this->server_3_.c_str());
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}
void SNTPComponent::update() {
#if !defined(USE_ESP_IDF) && !defined(USE_HOST)
  // force resync
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
  }
#endif
}
void SNTPComponent::loop() {
  for (const auto &item : callback_args_) {
    switch (item.second) {
      case SNTP_SYNC_STATUS_RESET:
        ESP_LOGD(TAG, "Time sync reset");
        break;
      case SNTP_SYNC_STATUS_COMPLETED:
        ESP_LOGD(TAG, "Time sync completed");
        this->time_sync_callback_.call();
        break;
      case SNTP_SYNC_STATUS_IN_PROGRESS:
        ESP_LOGD(TAG, "Time sync in progress");
        break;
    }
  }
  callback_args_.clear();

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
