#include "sntp_component.h"
#include "esphome/core/log.h"
#ifdef USE_SNTP_TIMEZONE_SERVICE
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/components/time/posix_tz.h"
#include <cctype>
#include <cinttypes>
#include <cstring>
#endif

#ifdef USE_ESP32
#include "esp_sntp.h"
#elif USE_ESP8266
#include "sntp.h"
#else
#include "lwip/apps/sntp.h"
#endif

namespace esphome::sntp {

static const char *const TAG = "sntp";

#ifdef USE_SNTP_TIMEZONE_SERVICE
static constexpr const char *TIME_NOW_API_URL = "https://time.now/developer/api/";
static constexpr const char *ZONE_IP = "ip";
static constexpr const char *TIMEZONE_REFRESH = "tz_refresh";
static constexpr const char *TIMEZONE_FETCH = "tz_fetch";
static constexpr uint32_t TIMEZONE_RETRY_MS = 60000;
// The service returns a few hundred bytes
static constexpr size_t TIMEZONE_RESPONSE_MAX = 768;

struct ZonePreference {
  char zone[SNTPComponent::MAX_ZONE_LENGTH + 1];
};

// Accept only characters found in tz database names, so the zone is safe to put in a URL path.
static bool is_valid_zone(StringRef zone) {
  if (zone.empty() || zone.size() > SNTPComponent::MAX_ZONE_LENGTH || zone[0] == '/')
    return false;
  for (char c : zone) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '/' && c != '_' && c != '-' && c != '+')
      return false;
  }
  return true;
}
#endif

#if defined(USE_ESP32)
SNTPComponent *SNTPComponent::instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

void SNTPComponent::setup() {
#ifdef USE_SNTP_TIMEZONE_SERVICE
  // The key includes the configured zone, so a zone set at runtime is dropped when the configuration changes
  this->zone_pref_ = global_preferences->make_preference<ZonePreference>(
      fnv1_hash_extend(fnv1_hash("sntp_timezone"), this->config_zone_), true);
  ZonePreference saved{};
  if (this->zone_pref_.load(&saved) && memchr(saved.zone, '\0', sizeof(saved.zone)) != nullptr &&
      is_valid_zone(StringRef(saved.zone))) {
    strcpy(this->zone_, saved.zone);  // NOLINT(clang-analyzer-security.insecureAPI.strcpy)
  } else {
    strncpy(this->zone_, this->config_zone_, MAX_ZONE_LENGTH);
    this->zone_[MAX_ZONE_LENGTH] = '\0';
  }
  this->set_interval(TIMEZONE_REFRESH, this->timezone_update_interval_, [this]() { this->fetch_timezone_(); });
#endif
#if defined(USE_ESP32)
  SNTPComponent::instance = this;
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  size_t i = 0;
  for (auto &server : this->servers_) {
    esp_sntp_setservername(i++, server);
  }
  esp_sntp_set_sync_interval(this->get_update_interval());
  esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
    if (SNTPComponent::instance != nullptr) {
      SNTPComponent::instance->defer([]() { SNTPComponent::instance->time_synced(); });
    }
  });
  esp_sntp_init();
#else
  sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);

  size_t i = 0;
  for (auto &server : this->servers_) {
    sntp_setservername(i++, server);
  }

#if defined(USE_ESP8266)
  settimeofday_cb([this](bool from_sntp) {
    if (from_sntp)
      this->time_synced();
  });
#endif

  sntp_init();
#endif
}
void SNTPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNTP Time:");
  size_t i = 0;
  for (auto &server : this->servers_) {
    ESP_LOGCONFIG(TAG, "  Server %zu: '%s'", i++, server);
  }
#ifdef USE_SNTP_TIMEZONE_SERVICE
  ESP_LOGCONFIG(TAG,
                "  Timezone service: time.now\n"
                "  Zone: %s\n"
                "  Timezone update interval: %" PRIu32 "s",
                this->zone_, this->timezone_update_interval_ / 1000);
#endif
  RealTimeClock::dump_config();
}
void SNTPComponent::update() {
#if !defined(USE_ESP32)
  // Some platforms currently cannot set the sync interval at runtime so we need
  // to do the re-sync by hand for now.
  if (sntp_enabled()) {
    sntp_stop();
    this->has_time_ = false;
    sntp_init();
  }
#endif
}
void SNTPComponent::loop() {
// The loop is used to infer whether we have valid time on platforms where we
// cannot tell whether SNTP has succeeded.
// One limitation of this approach is that we cannot tell if it was the SNTP
// component that set the time.
// ESP-IDF and ESP8266 use callbacks from the SNTP task to trigger the
// `on_time_sync` trigger on successful sync events.
#if defined(USE_ESP32) || defined(USE_ESP8266)
  this->disable_loop();
#endif

  if (this->has_time_)
    return;

  this->time_synced();
}

void SNTPComponent::time_synced() {
  auto time = this->now();
  this->has_time_ = time.is_valid();
  if (!this->has_time_)
    return;

  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  this->time_sync_callback_.call();
#ifdef USE_SNTP_TIMEZONE_SERVICE
  // Time sync shows the network is up, so this is a good moment for the first fetch.
  // A named timeout also moves the request out of the SNTP callback into the main loop.
  if (!this->timezone_fetched_)
    this->set_timeout(TIMEZONE_FETCH, 0, [this]() { this->fetch_timezone_(); });
#endif
}

#ifdef USE_SNTP_TIMEZONE_SERVICE
bool SNTPComponent::set_timezone(StringRef zone) {
  if (!is_valid_zone(zone)) {
    ESP_LOGW(TAG, "Invalid zone '%.*s'", (int) zone.size(), zone.c_str());
    return false;
  }
  size_t len = zone.copy(this->zone_, MAX_ZONE_LENGTH);
  this->zone_[len] = '\0';
  ZonePreference pref{};
  memcpy(pref.zone, this->zone_, len + 1);
  this->zone_pref_.save(&pref);
  ESP_LOGD(TAG, "Zone set to %s", this->zone_);
  this->set_timeout(TIMEZONE_FETCH, 0, [this]() { this->fetch_timezone_(); });
  return true;
}

void SNTPComponent::fetch_timezone_() {
  if (!network::is_connected()) {
    this->set_timeout(TIMEZONE_FETCH, TIMEZONE_RETRY_MS, [this]() { this->fetch_timezone_(); });
    return;
  }
  char url[96];
  if (strcmp(this->zone_, ZONE_IP) == 0) {
    snprintf(url, sizeof(url), "%sip", TIME_NOW_API_URL);
  } else {
    snprintf(url, sizeof(url), "%stimezone/%s", TIME_NOW_API_URL, this->zone_);
  }
  ESP_LOGD(TAG, "Fetching timezone from %s", url);

  bool ok = false;
  auto container = this->http_request_->get(url);
  if (container == nullptr) {
    ESP_LOGW(TAG, "Timezone request failed");
  } else if (!http_request::is_success(container->status_code)) {
    ESP_LOGW(TAG, "Timezone request failed with HTTP status %d", container->status_code);
    container->end();
  } else {
    uint8_t buf[TIMEZONE_RESPONSE_MAX];
    size_t len = 0;
    uint32_t last_data_time = millis();
    const uint32_t read_timeout = this->http_request_->get_timeout();
    bool complete = false;
    while (len < sizeof(buf)) {
      int read_or_error = container->read(buf + len, sizeof(buf) - len);
      App.feed_wdt();
      yield();
      auto result = http_request::http_read_loop_result(read_or_error, last_data_time, read_timeout,
                                                        container->is_read_complete());
      if (result == http_request::HttpReadLoopResult::RETRY)
        continue;
      if (result != http_request::HttpReadLoopResult::DATA) {
        complete = result == http_request::HttpReadLoopResult::COMPLETE;
        break;
      }
      len += read_or_error;
    }
    complete = complete || container->is_read_complete();
    container->end();
    if (!complete) {
      ESP_LOGW(TAG, "Timezone response incomplete or too large");
    } else {
      ok = this->apply_timezone_response_(buf, len);
    }
  }

  if (ok) {
    this->timezone_fetched_ = true;
    this->cancel_timeout(TIMEZONE_FETCH);
  } else {
    this->set_timeout(TIMEZONE_FETCH, TIMEZONE_RETRY_MS, [this]() { this->fetch_timezone_(); });
  }
}

bool SNTPComponent::apply_timezone_response_(const uint8_t *data, size_t len) {
  return json::parse_json(data, len, [this](JsonObject root) -> bool {
    if (!root["raw_offset"].is<int>() || !root["dst_offset"].is<int>() || !root["dst"].is<bool>()) {
      ESP_LOGW(TAG, "Timezone response has no valid raw_offset, dst_offset or dst");
      return false;
    }
    int32_t raw_offset = root["raw_offset"];
    int32_t dst_offset = root["dst"].as<bool>() ? root["dst_offset"].as<int32_t>() : 0;
    // Both offsets are seconds east of UTC; POSIX offsets are positive west, so negate.
    // The service gives only the current offset, not daylight saving rules; the periodic
    // refresh picks up the new offset after a daylight saving change.
    int32_t offset_seconds = -(raw_offset + dst_offset);
    time::ParsedTimezone tz{};
    tz.std_offset_seconds = offset_seconds;
    tz.dst_offset_seconds = offset_seconds;
    time::set_global_tz(tz);

    const char *abbreviation = root["abbreviation"] | "";
#ifdef USE_TEXT_SENSOR
    if (this->abbreviation_text_sensor_ != nullptr)
      this->abbreviation_text_sensor_->publish_state(abbreviation);
#endif

    ESP_LOGI(TAG, "Timezone %s, offset %" PRId32 "s, abbreviation %s", root["timezone"] | "", -offset_seconds,
             abbreviation);
    return true;
  });
}
#endif

}  // namespace esphome::sntp
