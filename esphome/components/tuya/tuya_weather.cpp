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

static constexpr uint32_t WEATHER_DEMAND_MIN_INTERVAL = 20 * 60 * 1000;
static constexpr uint32_t WEATHER_REQUEST_DEBOUNCE = 5000;
static constexpr uint32_t WEATHER_RETRY_INTERVAL = 30000;
static constexpr uint8_t WEATHER_MAX_RETRIES = 10;
static constexpr size_t WEATHER_MAX_PAYLOAD = 1024;
static constexpr size_t WEATHER_MAX_STRING = 99;
static constexpr size_t WEATHER_MAX_KEYS = 64;
static constexpr size_t WEATHER_MAX_VALUES = 128;

static bool weather_key_has_day(StringRef key) {
  size_t n = key.size();
  return n >= 3 && key[n - 2] == '.' && key[n - 1] >= '0' && key[n - 1] <= '6';
}

static bool weather_has_key(std::span<const TuyaWeatherValue> values, StringRef key) {
  return std::any_of(values.begin(), values.end(), [key](const TuyaWeatherValue &v) { return v.key == key; });
}

static bool weather_is_day_zero_of(StringRef candidate, StringRef key) {
  return candidate.size() == key.size() + 2 && candidate.starts_with(key) && candidate[key.size()] == '.' &&
         candidate[key.size() + 1] == '0';
}

static bool weather_has_day_zero(std::span<const TuyaWeatherValue> values, StringRef key) {
  return std::any_of(values.begin(), values.end(),
                     [key](const TuyaWeatherValue &v) { return weather_is_day_zero_of(StringRef(v.key), key); });
}

void TuyaUART::reset_weather_() {
  this->weather_enabled_ = false;
  this->weather_keys_.clear();
  this->weather_forecast_days_ = 0;
  this->cancel_interval(WEATHER_INTERVAL_ID);
  this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
  this->last_weather_sent_ = this->last_weather_demand_ = this->weather_retries_ = 0;
  this->weather_update_empty_ = this->weather_demand_pending_ = false;
  this->weather_last_complete_ = this->weather_in_flight_ = false;
  this->weather_frames_.clear();
  // a frame on the wire is written to its end from weather_buffer_ and then neither waited for nor sent again
  if (this->expected_response_ == TuyaCommandType::WEATHER_DATA)
    this->expected_response_.reset();
  std::erase_if(this->command_queue_, [](const TuyaCommand &cmd) { return cmd.cmd == TuyaCommandType::WEATHER_DATA; });
}

std::vector<uint8_t> TuyaUART::take_weather_buffer_() {
  std::vector<uint8_t> buffer;
  if (!this->weather_tx_active_)
    buffer.swap(this->weather_buffer_);
  return buffer;
}

void TuyaUART::keep_weather_buffer_(std::vector<uint8_t> &buffer) {
  if (!this->weather_tx_active_ && buffer.capacity() > this->weather_buffer_.capacity())
    this->weather_buffer_ = std::move(buffer);
}

void TuyaUART::add_pending_weather_(StringRef key, bool is_string, int32_t value_int, StringRef value_string) {
  if (this->pending_weather_count_ == this->pending_weather_.size()) {
    if (this->pending_weather_count_ == WEATHER_MAX_VALUES) {
      this->weather_incomplete_ = true;
      return;
    }
    this->pending_weather_.emplace_back();
  }
  TuyaWeatherValue &value = this->pending_weather_[this->pending_weather_count_++];
  value.key.assign(key.c_str(), key.size());
  value.is_string = is_string;
  value.value_int = value_int;
  value.value_string.assign(value_string.c_str(), value_string.size());
}

TuyaWeatherFrame TuyaUART::weather_answer_kind_() const {
  if (this->weather_incomplete_)
    return TuyaWeatherFrame::ANSWER;
  return this->weather_demand_pending_ ? TuyaWeatherFrame::DEMAND : TuyaWeatherFrame::COMPLETE;
}

void TuyaUART::handle_weather_enable_(const uint8_t *buffer, size_t len) {
  std::vector<std::string> keys;
  size_t i = 0;
  while (i < len) {
    size_t key_len = buffer[i];
    if (key_len == 0 || i + 1 + key_len > len || keys.size() == WEATHER_MAX_KEYS) {
      ESP_LOGW(TAG, "Weather enable request malformed at byte %u", (unsigned) i);
      this->reset_weather_();
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::WEATHER_ENABLE, .payload = std::vector<uint8_t>{0x00, 0x01}});
      return;
    }
    keys.emplace_back(reinterpret_cast<const char *>(buffer + i + 1), key_len);
    i += 1 + key_len;
  }
  this->weather_forecast_days_ = 0;
  for (const auto &key : keys) {
    if (key.size() == 8 && key.starts_with("w.date.") && key[7] >= '1' && key[7] <= '7')
      this->weather_forecast_days_ = key[7] - '0';
  }
  this->weather_keys_ = std::move(keys);
  this->weather_enabled_ = true;
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WEATHER_ENABLE, .payload = std::vector<uint8_t>{0x01, 0x00}});
  ESP_LOGI(TAG, "Weather service enabled by the MCU, %u keys", (unsigned) this->weather_keys_.size());
  for (const auto &key : this->weather_keys_) {
    ESP_LOGD(TAG, "  Weather key %s", key.c_str());
  }
  this->start_weather_interval_();
  // one answer waits at most: the next try sends the keys of this request
  if (this->init_state_ == TuyaInitState::INIT_DONE && this->weather_answer_queued_()) {
    this->start_weather_retry_();
  } else if (this->init_state_ == TuyaInitState::INIT_DONE) {
    this->answer_weather_enable_();
  }
}

void TuyaUART::answer_weather_enable_() {
  this->weather_retries_ = 0;
  if (this->request_weather_())
    return;
  if (!this->weather_incomplete_ && this->weather_request_callback_.size() == 0)
    return;
  ESP_LOGW(TAG, "Weather service enabled, no value to send yet");
  this->start_weather_retry_();
}

void TuyaUART::start_weather_retry_() {
  this->skip_weather_retry_ = true;
  this->set_interval(WEATHER_RETRY_INTERVAL_ID, WEATHER_RETRY_INTERVAL, [this] { this->retry_weather_(); });
}

void TuyaUART::retry_weather_() {
  if (this->skip_weather_retry_) {
    this->skip_weather_retry_ = false;
    return;
  }
  if (this->weather_answer_queued_())
    return;
  this->weather_retries_++;
  if (this->request_weather_())
    return;
  if (this->weather_retries_ >= WEATHER_MAX_RETRIES) {
    this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
    this->weather_demand_pending_ = false;
    ESP_LOGW(TAG, "Weather service enabled, no value to send after %u tries, giving up", WEATHER_MAX_RETRIES);
  }
}

void TuyaUART::handle_weather_request_() {
  uint32_t now = App.get_loop_component_start_time();
  const bool queued = this->weather_answer_queued_();
  bool answered =
      queued || (this->last_weather_sent_ != 0 && now - this->last_weather_sent_ < WEATHER_REQUEST_DEBOUNCE);
  bool refused =
      !answered && this->last_weather_demand_ != 0 && now - this->last_weather_demand_ < WEATHER_DEMAND_MIN_INTERVAL;
  std::vector<uint8_t> payload = this->take_weather_buffer_();
  bool failed = false;
  if (!answered && !refused) {
    if (!this->weather_enabled_) {
      this->weather_enabled_ = true;
      this->weather_forecast_days_ = this->configured_weather_days_();
      this->start_weather_interval_();
    }
    failed = !this->collect_weather_(payload);
  }
  this->send_command_(TuyaCommand{
      .cmd = TuyaCommandType::EXTENDED_SERVICES,
      .payload = std::vector<uint8_t>{static_cast<uint8_t>(TuyaExtendedServicesCommandType::REQUEST_WEATHER),
                                      static_cast<uint8_t>(refused || failed)}});
  if (refused || failed || answered)
    this->keep_weather_buffer_(payload);
  if (refused) {
    ESP_LOGD(TAG, "MCU asks for weather data again within 20 min, not sent");
    return;
  }
  if (failed) {
    if (!this->weather_update_empty_) {
      ESP_LOGW(TAG, "MCU asked for weather data, nothing to send");
    }
    this->weather_update_empty_ = true;
    return;
  }
  if (answered) {
    if (queued) {
      this->weather_demand_pending_ = true;
      for (TuyaWeatherFrame &kind : this->weather_frames_) {
        if (kind == TuyaWeatherFrame::COMPLETE)
          kind = TuyaWeatherFrame::DEMAND;
      }
    } else if (this->weather_last_complete_) {
      this->last_weather_demand_ = now;
    }
    return;
  }
  this->weather_update_empty_ = false;
  this->weather_retries_ = 0;
  this->weather_demand_pending_ = true;
  this->queue_weather_frame_(std::move(payload), this->weather_answer_kind_());
}

void TuyaUART::start_weather_interval_() {
  if (this->weather_update_interval_ == 0)
    return;
  this->skip_weather_update_ = true;
  this->set_interval(WEATHER_INTERVAL_ID, this->weather_update_interval_, [this] { this->update_weather_(); });
}

void TuyaUART::update_weather_() {
  if (this->skip_weather_update_) {
    this->skip_weather_update_ = false;
    return;
  }
  if (this->last_weather_sent_ != 0 &&
      App.get_loop_component_start_time() - this->last_weather_sent_ < this->weather_update_interval_ / 2)
    return;
  if (this->weather_answer_queued_())
    return;
  const bool empty = !this->request_weather_();
  if (empty && !this->weather_update_empty_) {
    ESP_LOGW(TAG, "Weather update: no value to send");
  }
  this->weather_update_empty_ = empty;
}

uint8_t TuyaUART::configured_weather_days_() const {
  uint8_t days = 0;
  auto note = [&days](const char *key) {
    if (weather_key_has_day(StringRef(key)))
      days = std::max<uint8_t>(days, key[strlen(key) - 1] - '0' + 1);
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

bool TuyaUART::weather_answer_queued_() const {
  return std::any_of(this->weather_frames_.begin(), this->weather_frames_.end(),
                     [](TuyaWeatherFrame kind) { return kind != TuyaWeatherFrame::VALUES; });
}

bool TuyaUART::weather_key_requested_(StringRef key) const {
  if (this->weather_keys_.empty())
    return true;
  if (std::find(this->weather_keys_.begin(), this->weather_keys_.end(), key) != this->weather_keys_.end())
    return true;
  size_t n = key.size();
  if (!weather_key_has_day(key))
    return false;
  if (key[n - 1] - '0' >= this->weather_forecast_days_)
    return false;
  const StringRef base(key.c_str(), n - 2);
  return std::find(this->weather_keys_.begin(), this->weather_keys_.end(), base) != this->weather_keys_.end();
}

// A source is left out when the MCU did not ask for its key, when an automation gave the value, and in a
// forecast when a value or a source for <key>.0 takes the place of the plain key
bool TuyaUART::weather_source_skipped_(const char *key) const {
  if (!this->weather_key_requested_(StringRef(key)) || weather_has_key(this->pending_weather_values_(), StringRef(key)))
    return true;
  if (this->weather_forecast_days_ == 0 || weather_key_has_day(StringRef(key)))
    return false;
  if (weather_has_day_zero(this->pending_weather_values_(), StringRef(key)))
    return true;
#ifdef USE_SENSOR
  for (const auto &src : this->weather_sensors_) {
    if (weather_is_day_zero_of(StringRef(src.first), StringRef(key)))
      return true;
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (const auto &src : this->weather_text_sensors_) {
    if (weather_is_day_zero_of(StringRef(src.first), StringRef(key)))
      return true;
  }
#endif
  return false;
}

bool TuyaUART::request_weather_() {
  std::vector<uint8_t> payload = this->take_weather_buffer_();
  if (!this->collect_weather_(payload)) {
    this->keep_weather_buffer_(payload);
    return false;
  }
  this->queue_weather_frame_(std::move(payload), this->weather_answer_kind_());
  return true;
}

void TuyaUART::queue_weather_frame_(std::vector<uint8_t> &&payload, TuyaWeatherFrame kind) {
  this->weather_frames_.push_back(kind);
  if (kind != TuyaWeatherFrame::VALUES)
    this->cancel_interval(WEATHER_RETRY_INTERVAL_ID);
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WEATHER_DATA, .payload = std::move(payload)});
}

void TuyaUART::handle_weather_ack_() {
  if (!this->weather_in_flight_ || this->weather_frames_.empty()) {
    ESP_LOGD(TAG, "Weather ack without a frame waiting for it, ignored");
    return;
  }
  this->weather_in_flight_ = false;
  TuyaWeatherFrame kind = this->weather_frames_.front();
  this->weather_frames_.erase(this->weather_frames_.begin());
  if (kind == TuyaWeatherFrame::VALUES)
    return;
  uint32_t now = App.get_loop_component_start_time();
  this->weather_retries_ = 0;
  this->last_weather_sent_ = now;
  this->weather_last_complete_ = kind != TuyaWeatherFrame::ANSWER;
  this->weather_demand_pending_ = false;
  if (kind == TuyaWeatherFrame::DEMAND)
    this->last_weather_demand_ = now;
}

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
    this->weather_demand_pending_ = false;
    ESP_LOGW(TAG, "Weather data not acknowledged after %u tries, giving up", WEATHER_MAX_RETRIES);
    return;
  }
  this->start_weather_retry_();
}

bool TuyaUART::collect_weather_(std::vector<uint8_t> &payload) {
  this->weather_incomplete_ = false;
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGD(TAG, "Weather data not sent, MCU not initialised yet");
    return false;
  }
  this->pending_weather_count_ = 0;
  this->collecting_weather_ = true;
  this->weather_request_callback_.call(this->weather_keys_);
  this->collecting_weather_ = false;

#ifdef USE_SENSOR
  for (auto &src : this->weather_sensors_) {
    if (this->weather_source_skipped_(src.first))
      continue;
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
      this->add_pending_weather_(StringRef(src.first), true, 0, StringRef(buf, n));
    } else {
      float rounded = roundf(state);
      if (rounded >= 2147483648.0f || rounded < -2147483648.0f) {
        ESP_LOGW(TAG, "Weather %s: %.0f is out of the int32 range, not sent", src.first, state);
        this->weather_incomplete_ = true;
        continue;
      }
      this->add_pending_weather_(StringRef(src.first), false, static_cast<int32_t>(rounded), StringRef());
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto &src : this->weather_text_sensors_) {
    if (this->weather_source_skipped_(src.first))
      continue;
    if (!src.second->has_state() || src.second->state.empty()) {
      ESP_LOGD(TAG, "Weather %s: '%s' has no value, not sent", src.first, LOG_STR_ARG(src.second->get_log_name()));
      this->weather_incomplete_ = true;
      continue;
    }
    this->add_pending_weather_(StringRef(src.first), true, 0, StringRef(src.second->state));
  }
#endif
  if (this->pending_weather_count_ == 0)
    return false;
  bool dropped = false;
  bool built = this->build_weather_payload_(this->pending_weather_values_(), payload, dropped);
  if (dropped)
    this->weather_incomplete_ = true;
  this->pending_weather_count_ = 0;
  return built;
}

bool TuyaUART::send_weather(const std::vector<TuyaWeatherValue> &values) {
  if (this->collecting_weather_) {
    for (const auto &v : values) {
      if (!weather_has_key(this->pending_weather_values_(), StringRef(v.key)))
        this->add_pending_weather_(StringRef(v.key), v.is_string, v.value_int, StringRef(v.value_string));
    }
    return true;
  }
  if (!this->weather_enabled_ || this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGW(TAG, "Weather data not sent, the MCU is not initialised or has not enabled the weather service");
    return false;
  }
  if (std::find(this->weather_frames_.begin(), this->weather_frames_.end(), TuyaWeatherFrame::VALUES) !=
      this->weather_frames_.end()) {
    ESP_LOGW(TAG, "Weather data not sent, the values sent before still wait for the MCU");
    return false;
  }
  std::vector<uint8_t> payload = this->take_weather_buffer_();
  bool dropped = false;
  if (!this->build_weather_payload_(values, payload, dropped)) {
    this->keep_weather_buffer_(payload);
    return false;
  }
  this->queue_weather_frame_(std::move(payload), TuyaWeatherFrame::VALUES);
  return true;
}

bool TuyaUART::build_weather_payload_(std::span<const TuyaWeatherValue> values, std::vector<uint8_t> &payload,
                                      bool &dropped) {
  payload.assign(1, 0x01);
  unsigned sent = 0;
  for (const auto &v : values) {
    if (!this->weather_key_requested_(StringRef(v.key))) {
      ESP_LOGV(TAG, "Weather key %s not requested by the MCU, skipped", v.key.c_str());
      continue;
    }
    bool today = this->weather_forecast_days_ != 0 && !weather_key_has_day(StringRef(v.key));
    if (today && weather_has_day_zero(values, StringRef(v.key)))
      continue;
    size_t key_size = v.key.size() + (today ? 2 : 0);
    size_t entry = 1 + key_size + 2 + (v.is_string ? v.value_string.size() : 4);
    if (key_size > 255 || v.value_string.size() > WEATHER_MAX_STRING || payload.size() + entry > WEATHER_MAX_PAYLOAD) {
      ESP_LOGW(TAG, "Weather %s: key or value too long or frame full, not sent", v.key.c_str());
      dropped = true;
      continue;
    }
    ESP_LOGD(TAG, "Weather %s: %" PRId32 " '%s'", v.key.c_str(), v.value_int, v.value_string.c_str());
    payload.push_back(static_cast<uint8_t>(key_size));
    payload.insert(payload.end(), v.key.begin(), v.key.end());
    if (today)
      payload.insert(payload.end(), {'.', '0'});
    if (v.is_string) {
      payload.insert(payload.end(), {0x01, static_cast<uint8_t>(v.value_string.size())});
      payload.insert(payload.end(), v.value_string.begin(), v.value_string.end());
    } else {
      const uint32_t u = static_cast<uint32_t>(v.value_int);
      payload.insert(payload.end(), {0x00, 0x04, uint8_t(u >> 24), uint8_t(u >> 16), uint8_t(u >> 8), uint8_t(u)});
    }
    sent++;
  }
  if (sent == 0) {
    ESP_LOGW(TAG, "Weather data not sent, no value is a key the MCU asked for and fits");
    return false;
  }
  ESP_LOGD(TAG, "Weather data: %u values, %u bytes", sent, (unsigned) payload.size());
  return true;
}

}  // namespace esphome::tuya

#endif  // USE_TUYA_WEATHER
