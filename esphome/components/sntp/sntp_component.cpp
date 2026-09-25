#include "sntp_component.h"
#include "esphome/core/log.h"
#ifdef USE_SNTP_TIMEZONE_SERVICE
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/components/time/posix_tz.h"
#include <cctype>
#include <cmath>
#include <cinttypes>
#include <cstdlib>
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
static constexpr const char *TIMEZONE_REFRESH = "tz_refresh";
static constexpr const char *TIMEZONE_FETCH = "tz_fetch";
static constexpr uint32_t TIMEZONE_RETRY_MS = 60000;
// Not in http_request's list of status codes
static constexpr int HTTP_STATUS_UNPROCESSABLE_CONTENT = 422;
// The service returns about 550 bytes
static constexpr size_t TIMEZONE_RESPONSE_MAX = 1024;
// Transition times are local seconds after midnight; POSIX allows -167 to +167 hours
static constexpr int32_t MAX_TRANSITION_SECONDS = 167 * 3600;
static constexpr int32_t MAX_OFFSET_SECONDS = 25 * 3600;
#ifdef USE_TEXT_SENSOR
static constexpr const char *TIMEZONE_ABBREVIATION = "tz_abbr";
// Daylight saving changes happen on a minute boundary, so checking each minute is enough
static constexpr uint32_t ABBREVIATION_CHECK_MS = 60000;
#endif

// A zone name, or if that is empty, a location
struct ZonePreference {
  char zone[SNTPComponent::MAX_ZONE_LENGTH + 1];
  float latitude;
  float longitude;
};

// Written so that NaN is rejected
static bool is_valid_location(float latitude, float longitude) {
  return std::abs(latitude) <= 90.0f && std::abs(longitude) <= 180.0f;
}

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

// Read one daylight saving rule from the service's response, checking every field is in range.
static bool read_dst_rule(JsonObjectConst obj, time::DSTRule &rule) {
  int type = obj["type"] | -1;
  int month = obj["month"] | -1;
  int week = obj["week"] | -1;
  int day_of_week = obj["day_of_week"] | -1;
  int day = obj["day"] | -1;
  JsonVariantConst time_seconds = obj["time_seconds"];
  if (type < 0 || type > static_cast<int>(time::DSTRuleType::DAY_OF_YEAR) || month < 0 || month > 12 || week < 0 ||
      week > 5 || day_of_week < 0 || day_of_week > 6 || day < 0 || day > 365 || !time_seconds.is<int32_t>() ||
      std::abs(time_seconds.as<int32_t>()) > MAX_TRANSITION_SECONDS)
    return false;
  rule.type = static_cast<time::DSTRuleType>(type);
  rule.month = month;
  rule.week = week;
  rule.day_of_week = day_of_week;
  rule.day = day;
  rule.time_seconds = time_seconds.as<int32_t>();
  return true;
}

static bool read_offset(JsonVariantConst value, int32_t &offset) {
  if (!value.is<int32_t>() || std::abs(value.as<int32_t>()) > MAX_OFFSET_SECONDS)
    return false;
  offset = value.as<int32_t>();
  return true;
}
#endif

#if defined(USE_ESP32)
SNTPComponent *SNTPComponent::instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

void SNTPComponent::setup() {
#ifdef USE_SNTP_TIMEZONE_SERVICE
  // The key includes the configured zone and location, so a zone set at runtime is dropped when the
  // configuration changes
  uint32_t key = fnv1_hash_extend(fnv1_hash("sntp_timezone"), this->config_zone_);
  key = fnv1_hash_extend(key, static_cast<int32_t>(this->config_latitude_ * 1e6f));
  key = fnv1_hash_extend(key, static_cast<int32_t>(this->config_longitude_ * 1e6f));
  this->zone_pref_ = global_preferences->make_preference<ZonePreference>(key, true);
  this->load_saved_or_config_();
  this->set_interval(TIMEZONE_REFRESH, this->timezone_update_interval_, [this]() { this->fetch_timezone_(); });
#ifdef USE_TEXT_SENSOR
  if (this->abbreviation_text_sensor_ != nullptr)
    this->set_interval(TIMEZONE_ABBREVIATION, ABBREVIATION_CHECK_MS, [this]() { this->publish_abbreviation_(false); });
#endif
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
  ESP_LOGCONFIG(TAG, "  Timezone service: %s", this->timezone_url_);
  if (this->zone_[0] != '\0') {
    ESP_LOGCONFIG(TAG, "  Zone: %s", this->zone_);
  } else {
    ESP_LOGCONFIG(TAG, "  Location: %.4f, %.4f", this->latitude_, this->longitude_);
  }
  ESP_LOGCONFIG(TAG, "  Timezone update interval: %" PRIu32 "s", this->timezone_update_interval_ / 1000);
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
  this->zone_save_pending_ = true;
  ESP_LOGD(TAG, "Zone set to %s", this->zone_);
  this->set_timeout(TIMEZONE_FETCH, 0, [this]() { this->fetch_timezone_(); });
  return true;
}

bool SNTPComponent::set_timezone(float latitude, float longitude) {
  if (!is_valid_location(latitude, longitude)) {
    ESP_LOGW(TAG, "Invalid location %.4f, %.4f", latitude, longitude);
    return false;
  }
  this->zone_[0] = '\0';
  this->latitude_ = latitude;
  this->longitude_ = longitude;
  this->zone_save_pending_ = true;
  ESP_LOGD(TAG, "Location set to %.4f, %.4f", latitude, longitude);
  this->set_timeout(TIMEZONE_FETCH, 0, [this]() { this->fetch_timezone_(); });
  return true;
}

void SNTPComponent::load_saved_or_config_() {
  ZonePreference saved{};
  if (this->zone_pref_.load(&saved) && memchr(saved.zone, '\0', sizeof(saved.zone)) != nullptr &&
      (saved.zone[0] != '\0' ? is_valid_zone(StringRef(saved.zone))
                             : is_valid_location(saved.latitude, saved.longitude))) {
    strcpy(this->zone_, saved.zone);  // NOLINT(clang-analyzer-security.insecureAPI.strcpy)
    this->latitude_ = saved.latitude;
    this->longitude_ = saved.longitude;
  } else {
    strncpy(this->zone_, this->config_zone_, MAX_ZONE_LENGTH);
    this->zone_[MAX_ZONE_LENGTH] = '\0';
    this->latitude_ = this->config_latitude_;
    this->longitude_ = this->config_longitude_;
  }
}

void SNTPComponent::fetch_timezone_() {
  if (!network::is_connected()) {
    this->set_timeout(TIMEZONE_FETCH, TIMEZONE_RETRY_MS, [this]() { this->fetch_timezone_(); });
    return;
  }
  // The zone only holds characters checked by is_valid_zone(), so it needs no escaping
  char body[80];
  if (this->zone_[0] != '\0') {
    snprintf(body, sizeof(body), R"({"zone":"%s"})", this->zone_);
  } else {
    snprintf(body, sizeof(body), R"({"latitude":%.6f,"longitude":%.6f})", this->latitude_, this->longitude_);
  }
  ESP_LOGD(TAG, "Fetching timezone for %s", body);

  bool ok = false;
  bool rejected = false;
  auto container = this->http_request_->post(this->timezone_url_, std::string(body),
                                             {http_request::Header{"Content-Type", "application/json"}});
  if (container == nullptr) {
    ESP_LOGW(TAG, "Timezone request failed");
  } else if (!http_request::is_success(container->status_code)) {
    // The service does not know the zone, or has no rules for it
    rejected = container->status_code == http_request::HTTP_STATUS_NOT_FOUND ||
               container->status_code == HTTP_STATUS_UNPROCESSABLE_CONTENT;
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
    if (this->zone_save_pending_) {
      this->zone_save_pending_ = false;
      ZonePreference pref{};
      memcpy(pref.zone, this->zone_, strlen(this->zone_) + 1);
      if (this->zone_[0] == '\0') {
        pref.latitude = this->latitude_;
        pref.longitude = this->longitude_;
      }
      this->zone_pref_.save(&pref);
    }
    this->cancel_timeout(TIMEZONE_FETCH);
  } else if (rejected) {
    // Asking again cannot help, so don't retry every minute
    this->cancel_timeout(TIMEZONE_FETCH);
    if (this->zone_save_pending_) {
      if (this->zone_[0] != '\0') {
        ESP_LOGW(TAG, "The service has no rules for zone '%s', going back to the previous zone", this->zone_);
      } else {
        ESP_LOGW(TAG, "The service has no rules for location %.4f, %.4f, going back to the previous zone",
                 this->latitude_, this->longitude_);
      }
      this->zone_save_pending_ = false;
      this->load_saved_or_config_();
      // The rules for the previous zone are still in use, unless none have been received yet
      if (!this->timezone_fetched_)
        this->set_timeout(TIMEZONE_FETCH, 0, [this]() { this->fetch_timezone_(); });
    } else {
      ESP_LOGE(TAG, "The service has no rules for the configured zone or location, will try again at the next update");
    }
  } else {
    this->set_timeout(TIMEZONE_FETCH, TIMEZONE_RETRY_MS, [this]() { this->fetch_timezone_(); });
  }
}

bool SNTPComponent::apply_timezone_response_(const uint8_t *data, size_t len) {
  return json::parse_json(data, len, [this](JsonObject root) -> bool {
    time::ParsedTimezone tz{};
    if (!read_offset(root["std_offset_seconds"], tz.std_offset_seconds) ||
        !read_offset(root["dst_offset_seconds"], tz.dst_offset_seconds) ||
        !read_dst_rule(root["dst_start"], tz.dst_start) || !read_dst_rule(root["dst_end"], tz.dst_end)) {
      ESP_LOGW(TAG, "Timezone response has missing or invalid rules");
      return false;
    }
    time::set_global_tz(tz);
    ESP_LOGI(TAG, "Timezone %s (%s)", root["zone"] | "", root["posix"] | "");

#ifdef USE_TEXT_SENSOR
    strncpy(this->std_abbreviation_, root["std_abbreviation"] | "", MAX_ABBREVIATION_LENGTH);
    this->std_abbreviation_[MAX_ABBREVIATION_LENGTH] = '\0';
    strncpy(this->dst_abbreviation_, root["dst_abbreviation"] | "", MAX_ABBREVIATION_LENGTH);
    this->dst_abbreviation_[MAX_ABBREVIATION_LENGTH] = '\0';
    this->publish_abbreviation_(true);
#endif
    return true;
  });
}

#ifdef USE_TEXT_SENSOR
void SNTPComponent::publish_abbreviation_(bool force) {
  // Nothing to publish until the service has given the abbreviations
  if (this->abbreviation_text_sensor_ == nullptr || (!force && !this->timezone_fetched_))
    return;
  bool is_dst = time::is_in_dst(this->timestamp_now(), time::get_global_tz());
  if (!force && is_dst == this->abbreviation_is_dst_)
    return;
  this->abbreviation_is_dst_ = is_dst;
  this->abbreviation_text_sensor_->publish_state(is_dst ? this->dst_abbreviation_ : this->std_abbreviation_);
}
#endif
#endif

}  // namespace esphome::sntp
