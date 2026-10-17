#include "pylontech.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>

// Helper macros
#define PARSE_INT(field, field_name) \
  { \
    get_token(token_buf); \
    auto val = parse_number<int>(token_buf); \
    if (val.has_value()) { \
      (field) = val.value(); \
    } else { \
      ESP_LOGV(TAG, "invalid " field_name " in line %s", buffer); \
      return; \
    } \
  }

#define PARSE_STR(field, field_name) \
  { \
    get_token(field); \
    if (strlen(field) < 2) { \
      ESP_LOGV(TAG, "too short " field_name " in line %s", buffer); \
      return; \
    } \
  }

namespace esphome::pylontech {

ESPHOME_LOG_TAG(TAG, "pylontech");

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "pylontech:");

#ifdef PYLONTECH_LISTENER_COUNT
  for (PylontechListener *listener : this->listeners_) {
    listener->dump_config();
  }
#endif

  LOG_UPDATE_INTERVAL(this);
}

void PylontechComponent::setup() {
  while (this->available() != 0) {
    this->read();
  }
}

void PylontechComponent::update() { this->write_str("pwr\n"); }

void PylontechComponent::loop() {
  // Drain the UART quickly; pylontech sends large bursts. Logging and publishing wait until it is idle.
  size_t avail = this->available();
#ifdef PYLONTECH_LISTENER_COUNT
  if (avail == 0) {
    this->dispatch_one_();
    return;
  }
#endif
  uint8_t buf[64];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;
#ifdef PYLONTECH_LISTENER_COUNT
    for (size_t i = 0; i < to_read; i++) {
      const char *line = this->line_reader_.feed(buf[i]);
      if (line != nullptr) {
        this->process_line_(line);
      }
    }
#endif
  }
}

#ifdef PYLONTECH_LISTENER_COUNT
void PylontechComponent::dispatch_one_() {
  if (this->dispatched_ == this->queue_.size()) {
    return;
  }
  PylontechListener::LineContents *l = &this->queue_[this->dispatched_++];
  ESP_LOGV(TAG, "Publishing battery %d", l->bat_num);
  for (PylontechListener *listener : this->listeners_) {
    listener->on_line_read(l);
  }
  if (this->dispatched_ == this->queue_.size()) {
    this->queue_.clear();
    this->dispatched_ = 0;
    if (this->dropped_ != 0) {
      ESP_LOGW(TAG, "Queue full, dropped %u lines", this->dropped_);
      this->dropped_ = 0;
    }
  }
}

// Runs while the UART drains: no logging above VERBOSE and no publishing here
void PylontechComponent::process_line_(const char *buffer) {
  // clang-format off
  // example lines to parse:
  // Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time                 B.V.St   B.T.St   MosTempr M.T.St
  // 1     50548  8910   25000  24200  25000  3368   3371   Charge   Normal   Normal   Normal   97%      2021-06-30 20:49:45  Normal  Normal  22700    Normal
  // 1     46012  1255   9100   5300   5500   3047   3091   SysError Low      Normal   Normal   4%       2025-11-28 17:56:33  Low      Normal  7800     Normal
  // newer firmware example:
  // Power Volt Curr Tempr Tlow Tlow.Id Thigh Thigh.Id Vlow Vlow.Id Vhigh Vhigh.Id Base.St Volt.St Curr.St Temp.St Coulomb Time                B.V.St B.T.St MosTempr M.T.St SysAlarm.St
  // 1     49405 0   17600 13700 8      14500 0        3293 2       3294   0       Idle    Normal  Normal  Normal  60%     2025-12-05 00:53:41 Normal Normal 16600    Normal Normal
  // clang-format on

  PylontechListener::LineContents l{};

  const char *cursor = buffer;
  char token_buf[TEXT_SENSOR_MAX_LEN] = {0};

  // Helper Lambda to extract tokens
  auto get_token = [&](char *token_buf) -> void {
    // Skip leading whitespace
    while (*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }

    if (*cursor == '\0') {
      token_buf[0] = 0;
      return;
    }

    const char *start = cursor;

    // Find end of field
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }

    size_t token_len = std::min(static_cast<size_t>(cursor - start), static_cast<size_t>(TEXT_SENSOR_MAX_LEN - 1));
    memcpy(token_buf, start, token_len);
    token_buf[token_len] = 0;
  };

  {
    get_token(token_buf);
    auto val = parse_number<int>(token_buf);
    if (val.has_value() && val.value() > 0) {
      l.bat_num = val.value();
      if (l.bat_num > 16 || (this->batteries_ & (1u << (l.bat_num - 1))) == 0) {
        return;  // no listener for this battery
      }
    } else if (strcmp(token_buf, "Power") == 0) {
      // header line i.e. "Power Volt   Curr" and so on
      this->has_tlow_id_ = strstr(buffer, "Tlow.Id") != nullptr;
      ESP_LOGV(TAG, "header line %s Tlow.Id: %s",
               this->has_tlow_id_ ? LOG_STR_LITERAL("with") : LOG_STR_LITERAL("without"), buffer);
      return;
    } else {
      ESP_LOGV(TAG, "unknown line %s", buffer);
      return;
    }
  }
  PARSE_INT(l.volt, "Volt");
  PARSE_INT(l.curr, "Curr");
  PARSE_INT(l.tempr, "Tempr");
  PARSE_INT(l.tlow, "Tlow");
  if (this->has_tlow_id_) {
    get_token(token_buf);  // Skip Tlow.Id
  }
  PARSE_INT(l.thigh, "Thigh");
  if (this->has_tlow_id_) {
    get_token(token_buf);  // Skip Thigh.Id
  }
  PARSE_INT(l.vlow, "Vlow");
  if (this->has_tlow_id_) {
    get_token(token_buf);  // Skip Vlow.Id
  }
  PARSE_INT(l.vhigh, "Vhigh");
  if (this->has_tlow_id_) {
    get_token(token_buf);  // Skip Vhigh.Id
  }
  PARSE_STR(l.base_st, "Base.St");
  PARSE_STR(l.volt_st, "Volt.St");
  PARSE_STR(l.curr_st, "Curr.St");
  PARSE_STR(l.temp_st, "Temp.St");
  {
    get_token(token_buf);
    for (char &i : token_buf) {
      if (i == '%') {
        i = 0;
        break;
      }
    }
    auto coul_val = parse_number<int>(token_buf);
    if (coul_val.has_value()) {
      l.coulomb = coul_val.value();
    } else {
      ESP_LOGV(TAG, "invalid Coulomb in line %s", buffer);
      return;
    }
  }
  get_token(token_buf);  // Skip Date
  get_token(token_buf);  // Skip Time
  get_token(token_buf);  // Skip B.V.St
  get_token(token_buf);  // Skip B.T.St
  PARSE_INT(l.mostempr, "Mostempr");

  if (this->queue_.size() == PYLONTECH_BATTERY_COUNT) {
    if (this->dropped_ != UINT8_MAX) {
      this->dropped_++;
    }
    return;
  }
  this->queue_.push_back(l);
}
#endif

}  // namespace esphome::pylontech

#undef PARSE_INT
#undef PARSE_STR
