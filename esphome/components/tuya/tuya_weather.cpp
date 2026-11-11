#include "esphome/core/defines.h"

// only with the UART transport, which sets the define
#ifdef USE_TUYA_WEATHER
#include "tuya_uart.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::tuya {

ESPHOME_LOG_TAG(TAG, "tuya");

// 0x20: MCU enables the service with a key list. 0x21: module sends values. 0x34/0x03: MCU asks for data now.
// An empty key list is taken as every configured key.
// Tuya: an on-demand weather request (0x34/0x03) is served at most once per 20 min.
static constexpr uint32_t WEATHER_DEMAND_MIN_INTERVAL = 20 * 60 * 1000;
// The MCU sends 0x20 and 0x34/0x03 back to back; one request callback answers both.
static constexpr uint32_t WEATHER_REQUEST_DEBOUNCE = 5000;
// An enable request that found no value is tried again at this interval
static constexpr uint32_t WEATHER_RETRY_INTERVAL = 30000;
static constexpr uint8_t WEATHER_MAX_RETRIES = 10;
static constexpr size_t WEATHER_MAX_PAYLOAD = 1024;
// Tuya's MCU SDK copies a string value into a 100 byte buffer
static constexpr size_t WEATHER_MAX_STRING = 99;
// Requested keys in the debug line
static constexpr size_t WEATHER_KEY_LOG_SIZE = 256;
// Keys of one enable request; Tuya's MCU SDK knows fewer than 30
static constexpr size_t WEATHER_MAX_KEYS = 64;

// w.<key>.<day>, day 0 = today
static bool weather_key_has_day(const std::string &key) {
  size_t n = key.size();
  return n >= 3 && key[n - 2] == '.' && key[n - 1] >= '0' && key[n - 1] <= '6';
}

static bool weather_has_key(const std::vector<TuyaWeatherValue> &values, const char *key) {
  for (const auto &v : values) {
    if (v.key == key)
      return true;
  }
  return false;
}

void TuyaUART::dump_weather_config_() {
  if (this->weather_update_interval_ != 0) {
    ESP_LOGCONFIG(TAG, "  Weather service: on the MCU's requests and every %" PRIu32 " s; %s by the MCU, %u keys",
                  this->weather_update_interval_ / 1000,
                  this->weather_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("not yet enabled"),
                  (unsigned) this->weather_keys_.size());
  } else {
    ESP_LOGCONFIG(TAG, "  Weather service: on the MCU's requests only; %s by the MCU, %u keys",
                  this->weather_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("not yet enabled"),
                  (unsigned) this->weather_keys_.size());
  }
#ifdef USE_SENSOR
  for (auto &src : this->weather_sensors_) {
    ESP_LOGCONFIG(TAG, "    %s from sensor '%s'", src.first, LOG_STR_ARG(src.second->get_log_name()));
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto &src : this->weather_text_sensors_) {
    ESP_LOGCONFIG(TAG, "    %s from text sensor '%s'", src.first, LOG_STR_ARG(src.second->get_log_name()));
  }
#endif
}

// The MCU enables the weather service again after a restart
void TuyaUART::reset_weather_() {
  this->weather_enabled_ = false;
  this->weather_keys_.clear();
  this->weather_forecast_days_ = 0;
  this->cancel_interval(WEATHER_INTERVAL_ID);
  this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
  this->last_weather_sent_ = 0;
  this->last_weather_demand_ = 0;
  this->weather_retries_ = 0;
  this->weather_update_empty_ = false;
  this->weather_frames_.clear();
  this->weather_in_flight_ = false;
  // frames for the service the MCU had enabled before
  if (this->expected_response_ == TuyaCommandType::WEATHER_DATA)
    this->expected_response_.reset();
  std::erase_if(this->command_queue_, [](const TuyaCommand &cmd) { return cmd.cmd == TuyaCommandType::WEATHER_DATA; });
}

void TuyaUART::handle_weather_enable_(const uint8_t *buffer, size_t len) {
  std::vector<std::string> keys;
  size_t i = 0;
  while (i < len) {
    size_t key_len = buffer[i];
    if (key_len == 0 || i + 1 + key_len > len || keys.size() == WEATHER_MAX_KEYS) {
      ESP_LOGW(TAG, "Weather enable request malformed at byte %u", (unsigned) i);
      // the key list of an earlier request no longer holds
      this->reset_weather_();
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::WEATHER_ENABLE, .payload = std::vector<uint8_t>{0x00, 0x01}});
      return;
    }
    keys.emplace_back(reinterpret_cast<const char *>(buffer + i + 1), key_len);
    i += 1 + key_len;
  }
  // w.date.N asks for the forecast too: every value key then also comes as w.<key>.<day>, day 0 = today .. N-1
  this->weather_forecast_days_ = 0;
  for (const auto &key : keys) {
    if (key.size() == 8 && key.starts_with("w.date.") && key[7] >= '1' && key[7] <= '7')
      this->weather_forecast_days_ = key[7] - '0';
  }
  this->weather_keys_ = std::move(keys);
  this->weather_enabled_ = true;
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WEATHER_ENABLE, .payload = std::vector<uint8_t>{0x01, 0x00}});
  ESP_LOGI(TAG, "Weather service enabled by the MCU, %u keys", (unsigned) this->weather_keys_.size());
  this->start_weather_interval_();
  // a request made during the initialization is answered when it is done
  if (this->init_state_ == TuyaInitState::INIT_DONE)
    this->answer_weather_enable_();
}

// First values after 0x20; without a frame the request is repeated until one goes out.
void TuyaUART::answer_weather_enable_() {
  this->weather_retries_ = 0;
  if (this->request_weather_())
    return;
  // nothing configured that could answer: values come from send_weather() calls only
  if (!this->weather_incomplete_ && this->weather_request_callback_.size() == 0) {
    size_t sources = 0;
#ifdef USE_SENSOR
    sources += this->weather_sensors_.size();
#endif
#ifdef USE_TEXT_SENSOR
    sources += this->weather_text_sensors_.size();
#endif
    if (sources != 0) {
      ESP_LOGW(TAG, "Weather service enabled, the MCU asked for none of the %u configured keys", (unsigned) sources);
    }
    return;
  }
  ESP_LOGW(TAG, "Weather service enabled, no value to send yet");
  this->start_weather_retry_();
}

// An answer that found no value or got no ack is built again, at most WEATHER_MAX_RETRIES times.
void TuyaUART::start_weather_retry_() {
  this->skip_weather_retry_ = true;
  this->set_interval(WEATHER_RETRY_INTERVAL_ID, WEATHER_RETRY_INTERVAL, [this] { this->retry_weather_(); });
}

void TuyaUART::retry_weather_() {
  // set_interval runs first after a short random offset: the first try is one full interval later
  if (this->skip_weather_retry_) {
    this->skip_weather_retry_ = false;
    return;
  }
  // its ack or its loss decides
  if (this->weather_answer_queued_())
    return;
  this->weather_retries_++;
  // a queued frame ends the retry
  if (this->request_weather_())
    return;
  if (this->weather_retries_ >= WEATHER_MAX_RETRIES) {
    this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
    ESP_LOGW(TAG, "Weather service enabled, no value to send after %u tries, giving up", WEATHER_MAX_RETRIES);
  }
}

// 0x34/0x03: the result byte goes out first, then the frame.
void TuyaUART::handle_weather_request_() {
  uint32_t now = App.get_loop_component_start_time();
  // an answer is on its way or was acknowledged within the last 5 s (the MCU sends 0x20 and 0x34/0x03 back to back)
  bool answered = this->weather_answer_queued_() ||
                  (this->last_weather_sent_ != 0 && now - this->last_weather_sent_ < WEATHER_REQUEST_DEBOUNCE);
  bool refused =
      !answered && this->last_weather_demand_ != 0 && now - this->last_weather_demand_ < WEATHER_DEMAND_MIN_INTERVAL;
  std::vector<uint8_t> payload;
  bool failed = false;
  if (!answered && !refused) {
    if (!this->weather_enabled_) {
      // the MCU sends its key list once per start of its own: after a restart of the module alone
      // every configured key is sent, as a forecast when forecast keys are configured
      this->weather_enabled_ = true;
      this->weather_forecast_days_ = this->configured_weather_days_();
      this->start_weather_interval_();
    }
    failed = !this->collect_weather_(payload);
  }
  // result byte: 0x00 success, 0x01 failure
  this->send_command_(TuyaCommand{
      .cmd = TuyaCommandType::EXTENDED_SERVICES,
      .payload = std::vector<uint8_t>{static_cast<uint8_t>(TuyaExtendedServicesCommandType::REQUEST_WEATHER),
                                      static_cast<uint8_t>(refused || failed)}});
  if (refused) {
    ESP_LOGD(TAG, "MCU asks for weather data, last request %" PRIu32 " s ago, not repeated",
             (now - this->last_weather_demand_) / 1000);
    return;
  }
  if (failed) {
    if (this->init_state_ != TuyaInitState::INIT_DONE) {
      ESP_LOGW(TAG, "MCU asked for weather data before it is initialised, not sent");
    } else if (!this->weather_update_empty_) {
      // once per streak, the MCU asks again and again
      this->weather_update_empty_ = true;
      ESP_LOGW(TAG, "MCU asked for weather data, nothing to send");
    } else {
      ESP_LOGD(TAG, "MCU asked for weather data, nothing to send");
    }
    return;
  }
  if (answered)
    return;
  this->weather_update_empty_ = false;
  this->weather_retries_ = 0;
  // only a complete answer counts against the 20 min limit, and only once the MCU acknowledged it
  this->queue_weather_frame_(std::move(payload),
                             this->weather_incomplete_ ? TuyaWeatherFrame::ANSWER : TuyaWeatherFrame::DEMAND);
}

// Unrequested frames while the MCU has the service enabled.
void TuyaUART::start_weather_interval_() {
  if (this->weather_update_interval_ == 0)
    return;
  this->skip_weather_update_ = true;
  this->set_interval(WEATHER_INTERVAL_ID, this->weather_update_interval_, [this] { this->update_weather_(); });
}

void TuyaUART::update_weather_() {
  // set_interval runs first after a short random offset: the first frame is one full interval later
  if (this->skip_weather_update_) {
    this->skip_weather_update_ = false;
    return;
  }
  // a request of the MCU was answered less than half an interval ago
  if (this->last_weather_sent_ != 0 &&
      App.get_loop_component_start_time() - this->last_weather_sent_ < this->weather_update_interval_ / 2)
    return;
  if (this->weather_answer_queued_())
    return;
  if (this->request_weather_()) {
    this->weather_update_empty_ = false;
    return;
  }
  // once per streak
  if (!this->weather_update_empty_) {
    this->weather_update_empty_ = true;
    ESP_LOGW(TAG, "Weather update: no value to send");
  }
}

// Forecast days of the configured sources: highest day of a w.<key>.<day> key plus one, 0 without such a key.
uint8_t TuyaUART::configured_weather_days_() const {
  uint8_t days = 0;
  auto note = [&days](const char *key) {
    const size_t n = strlen(key);
    if (n >= 3 && key[n - 2] == '.' && key[n - 1] >= '0' && key[n - 1] <= '6')
      days = std::max<uint8_t>(days, key[n - 1] - '0' + 1);
  };
#ifdef USE_SENSOR
  for (const auto &src : this->weather_sensors_)
    note(src.first);
#endif
#ifdef USE_TEXT_SENSOR
  for (const auto &src : this->weather_text_sensors_)
    note(src.first);
#endif
  (void) note;
  return days;
}

// A collected answer is queued or waits for its ack; frames of send_weather() calls outside a request do not count.
bool TuyaUART::weather_answer_queued_() const {
  for (TuyaWeatherFrame kind : this->weather_frames_) {
    if (kind != TuyaWeatherFrame::VALUES)
      return true;
  }
  return false;
}

// A forecast key w.<key>.<day> counts when the MCU asked for w.<key> and for more than <day> days (w.date.N).
bool TuyaUART::weather_key_requested_(const std::string &key) const {
  if (this->weather_keys_.empty())
    return true;
  if (std::find(this->weather_keys_.begin(), this->weather_keys_.end(), key) != this->weather_keys_.end())
    return true;
  size_t n = key.size();
  if (!weather_key_has_day(key))
    return false;
  if (key[n - 1] - '0' >= this->weather_forecast_days_)
    return false;
  std::string base = key.substr(0, n - 2);
  return std::find(this->weather_keys_.begin(), this->weather_keys_.end(), base) != this->weather_keys_.end();
}

bool TuyaUART::request_weather_() {
  std::vector<uint8_t> payload;
  if (!this->collect_weather_(payload))
    return false;
  this->queue_weather_frame_(std::move(payload), TuyaWeatherFrame::ANSWER);
  return true;
}

void TuyaUART::queue_weather_frame_(std::vector<uint8_t> &&payload, TuyaWeatherFrame kind) {
  // before the command: it may be written at once
  this->weather_frames_.push_back(kind);
  // values of a send_weather() call do not answer the request the retry is for
  if (kind != TuyaWeatherFrame::VALUES)
    this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WEATHER_DATA, .payload = std::move(payload)});
}

// Empty 0x21 from the MCU: the frame arrived.
void TuyaUART::handle_weather_ack_() {
  // a late ack of a frame that was already given up
  if (!this->weather_in_flight_ || this->weather_frames_.empty()) {
    ESP_LOGD(TAG, "Weather ack without a frame waiting for it, ignored");
    return;
  }
  this->weather_in_flight_ = false;
  TuyaWeatherFrame kind = this->weather_frames_.front();
  this->weather_frames_.erase(this->weather_frames_.begin());
  ESP_LOGV(TAG, "Weather data acknowledged by the MCU");
  if (kind == TuyaWeatherFrame::VALUES)
    return;
  uint32_t now = App.get_loop_component_start_time();
  this->weather_retries_ = 0;
  this->last_weather_sent_ = now;
  if (kind == TuyaWeatherFrame::DEMAND)
    this->last_weather_demand_ = now;
}

// A 0x21 frame got no ack: a collected answer is built again later.
void TuyaUART::handle_weather_lost_() {
  ESP_LOGW(TAG, "Weather data not acknowledged by the MCU");
  this->weather_in_flight_ = false;
  if (this->weather_frames_.empty())
    return;
  TuyaWeatherFrame kind = this->weather_frames_.front();
  this->weather_frames_.erase(this->weather_frames_.begin());
  if (kind == TuyaWeatherFrame::VALUES)
    return;
  if (this->weather_retries_ >= WEATHER_MAX_RETRIES) {
    ESP_LOGW(TAG, "Weather data not acknowledged after %u tries, giving up", WEATHER_MAX_RETRIES);
    return;
  }
  this->start_weather_retry_();
}

// Values of a request: automations first (their send_weather() calls are collected), then the configured sources
// for the keys they left out, one 0x21 payload in all.
bool TuyaUART::collect_weather_(std::vector<uint8_t> &payload) {
  this->weather_incomplete_ = false;
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGD(TAG, "Weather data not sent, MCU not initialised yet");
    return false;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  // a list that does not fit is cut at a whole key and marked
  char list[WEATHER_KEY_LOG_SIZE];
  size_t used = 0;
  bool cut = false;
  list[0] = '\0';
  for (const auto &key : this->weather_keys_) {
    int n = snprintf(list + used, sizeof(list) - used, "%s%s", used == 0 ? "" : ", ", key.c_str());
    if (n < 0 || static_cast<size_t>(n) >= sizeof(list) - used) {
      list[used] = '\0';
      cut = true;
      break;
    }
    used += n;
  }
  ESP_LOGD(TAG, "Weather data requested: %s%s", list, cut ? LOG_STR_LITERAL(" ... (list cut)") : LOG_STR_LITERAL(""));
#endif
  // pending_weather_ keeps its storage from one request to the next
  std::vector<TuyaWeatherValue> &values = this->pending_weather_;
  values.clear();
  this->collecting_weather_ = true;
  this->weather_request_callback_.call(this->weather_keys_);
  this->collecting_weather_ = false;

#ifdef USE_SENSOR
  for (auto &src : this->weather_sensors_) {
    // a source for a key the MCU did not ask for does not make the answer incomplete
    if (!this->weather_key_requested_(src.first))
      continue;
    if (weather_has_key(values, src.first))
      continue;  // answered by an automation
    float state = src.second->state;
    if (!std::isfinite(state)) {
      ESP_LOGD(TAG, "Weather %s: '%s' has no value, not sent", src.first, LOG_STR_ARG(src.second->get_log_name()));
      this->weather_incomplete_ = true;
      continue;
    }
    if (strncmp(src.first, "w.windSpeed", 11) == 0) {  // also the forecast keys w.windSpeed.<day>
      char buf[16];
      int n = snprintf(buf, sizeof(buf), "%.1f", state);
      if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        ESP_LOGW(TAG, "Weather %s: %.0f does not fit, not sent", src.first, state);
        this->weather_incomplete_ = true;
        continue;
      }
      values.push_back(TuyaWeatherValue{src.first, true, 0, buf});
    } else {
      float rounded = roundf(state);
      if (rounded >= 2147483648.0f || rounded < -2147483648.0f) {
        ESP_LOGW(TAG, "Weather %s: %.0f is out of the int32 range, not sent", src.first, state);
        this->weather_incomplete_ = true;
        continue;
      }
      values.push_back(TuyaWeatherValue{src.first, false, static_cast<int32_t>(rounded), {}});
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto &src : this->weather_text_sensors_) {
    if (!this->weather_key_requested_(src.first))
      continue;
    if (weather_has_key(values, src.first))
      continue;
    if (!src.second->has_state() || src.second->state.empty()) {
      ESP_LOGD(TAG, "Weather %s: '%s' has no value, not sent", src.first, LOG_STR_ARG(src.second->get_log_name()));
      this->weather_incomplete_ = true;
      continue;
    }
    values.push_back(TuyaWeatherValue{src.first, true, 0, src.second->state});
  }
#endif
  if (values.empty()) {
    ESP_LOGD(TAG, "Weather data requested, nothing to send");
    return false;
  }
  bool dropped = false;
  bool built = this->build_weather_payload_(values, payload, dropped);
  if (dropped)
    this->weather_incomplete_ = true;
  values.clear();
  return built;
}

bool TuyaUART::send_weather(const std::vector<TuyaWeatherValue> &values) {
  if (this->collecting_weather_) {
    for (const auto &v : values) {
      if (!weather_has_key(this->pending_weather_, v.key.c_str()))
        this->pending_weather_.push_back(v);
    }
    return true;
  }
  // outside a request: a frame of its own
  if (!this->weather_enabled_) {
    ESP_LOGW(TAG, "Weather data not sent, the MCU has not enabled the weather service");
    return false;
  }
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGW(TAG, "Weather data not sent, MCU not initialised yet");
    return false;
  }
  std::vector<uint8_t> payload;
  bool dropped = false;
  if (!this->build_weather_payload_(values, payload, dropped))
    return false;
  this->queue_weather_frame_(std::move(payload), TuyaWeatherFrame::VALUES);
  return true;
}

// Sets dropped when a value was left out for its size.
bool TuyaUART::build_weather_payload_(const std::vector<TuyaWeatherValue> &values, std::vector<uint8_t> &payload,
                                      bool &dropped) {
  payload.assign(1, 0x01);
  unsigned sent = 0;
  for (const auto &v : values) {
    if (!this->weather_key_requested_(v.key)) {
      ESP_LOGV(TAG, "Weather key %s not requested by the MCU, skipped", v.key.c_str());
      continue;
    }
    // an MCU that asked for a forecast (w.date.N) reads every key as w.<key>.<day>, today is day 0
    bool today = this->weather_forecast_days_ != 0 && !weather_key_has_day(v.key);
    if (today && weather_has_key(values, (v.key + ".0").c_str()))
      continue;
    size_t key_size = v.key.size() + (today ? 2 : 0);
    if (key_size > 255 || v.value_string.size() > WEATHER_MAX_STRING) {
      ESP_LOGW(TAG, "Weather %s: key or value too long, not sent", v.key.c_str());
      dropped = true;
      continue;
    }
    size_t entry = 1 + key_size + 2 + (v.is_string ? v.value_string.size() : 4);
    if (payload.size() + entry > WEATHER_MAX_PAYLOAD) {
      ESP_LOGW(TAG, "Weather %s: frame full, not sent", v.key.c_str());
      dropped = true;
      continue;
    }
    if (v.is_string) {
      ESP_LOGD(TAG, "Weather %s: '%s'", v.key.c_str(), v.value_string.c_str());
    } else {
      ESP_LOGD(TAG, "Weather %s: %" PRId32, v.key.c_str(), v.value_int);
    }
    payload.push_back(static_cast<uint8_t>(key_size));
    payload.insert(payload.end(), v.key.begin(), v.key.end());
    if (today) {
      payload.push_back('.');
      payload.push_back('0');
    }
    if (v.is_string) {
      payload.push_back(0x01);
      payload.push_back(static_cast<uint8_t>(v.value_string.size()));
      payload.insert(payload.end(), v.value_string.begin(), v.value_string.end());
    } else {
      uint32_t u = static_cast<uint32_t>(v.value_int);
      payload.push_back(0x00);
      payload.push_back(0x04);
      payload.push_back(u >> 24);
      payload.push_back(u >> 16);
      payload.push_back(u >> 8);
      payload.push_back(u);
    }
    sent++;
  }
  if (sent == 0) {
    ESP_LOGW(TAG, "Weather data not sent, none of the %u values is a key the MCU asked for and fits",
             (unsigned) values.size());
    return false;
  }
  ESP_LOGD(TAG, "Weather data: %u values, %u bytes", sent, (unsigned) payload.size());
  return true;
}

}  // namespace esphome::tuya

#endif  // USE_TUYA_WEATHER
