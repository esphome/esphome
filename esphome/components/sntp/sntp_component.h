#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include <array>
#ifdef USE_SNTP_TIMEZONE_SERVICE
#include "esphome/components/http_request/http_request.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#endif

namespace esphome::sntp {

// Server count is calculated at compile time by Python codegen
// SNTP_SERVER_COUNT will always be defined

/// The SNTP component allows you to configure local timekeeping via Simple Network Time Protocol.
///
/// \note
/// The C library (newlib) available on ESPs only supports TZ strings that specify an offset and DST info;
/// you cannot specify zone names or paths to zoneinfo files.
/// \see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
class SNTPComponent final : public time::RealTimeClock {
 public:
  SNTPComponent(const std::array<const char *, SNTP_SERVER_COUNT> &servers) : servers_(servers) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void update() override;
  void loop() override;

  void time_synced();

#ifdef USE_SNTP_TIMEZONE_SERVICE
  /// Longest zone name accepted, e.g. "America/Argentina/ComodRivadavia" is 32 characters.
  static constexpr size_t MAX_ZONE_LENGTH = 47;

  /// Fetch the timezone from the time.now service.
  /// @param zone A Region/City name, or "ip" to look up the zone from the device's public IP address.
  /// @param update_interval How often to fetch it again, in milliseconds, so daylight saving changes are applied.
  void set_timezone_service(http_request::HttpRequestComponent *http_request, const char *zone,
                            uint32_t update_interval) {
    this->http_request_ = http_request;
    this->config_zone_ = zone;
    this->timezone_update_interval_ = update_interval;
  }

  /// Change the zone at runtime and fetch it now. The new zone is kept across reboots until the
  /// zone in the configuration is changed. Returns false if the name is not a valid zone name.
  bool set_timezone(StringRef zone);

#ifdef USE_TEXT_SENSOR
  void set_timezone_abbreviation_text_sensor(text_sensor::TextSensor *text_sensor) {
    this->abbreviation_text_sensor_ = text_sensor;
  }
#endif
#endif

 protected:
  // Store const char pointers to string literals
  // ESP8266: strings in rodata (RAM), but avoids std::string overhead (~24 bytes each)
  // Other platforms: strings in flash
  std::array<const char *, SNTP_SERVER_COUNT> servers_;
  bool has_time_{false};

#ifdef USE_SNTP_TIMEZONE_SERVICE
  void fetch_timezone_();
  bool apply_timezone_response_(const uint8_t *data, size_t len);

  http_request::HttpRequestComponent *http_request_{nullptr};
  const char *config_zone_{""};
  uint32_t timezone_update_interval_{0};
  ESPPreferenceObject zone_pref_;
  char zone_[MAX_ZONE_LENGTH + 1]{};
  bool timezone_fetched_{false};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *abbreviation_text_sensor_{nullptr};
#endif
#endif

#if defined(USE_ESP32)
 private:
  static SNTPComponent *instance;
#endif
};

#ifdef USE_SNTP_TIMEZONE_SERVICE
template<typename... Ts> class SetTimezoneAction final : public Action<Ts...>, public Parented<SNTPComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, zone)

  void play(const Ts &...x) override { this->parent_->set_timezone(StringRef(this->zone_.value(x...))); }
};
#endif

}  // namespace esphome::sntp
