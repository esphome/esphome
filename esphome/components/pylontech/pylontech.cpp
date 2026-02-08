#include "pylontech.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Helper macros
#define PARSE_INT(field, field_name) \
  { \
    get_token(token_buf); \
    auto val = parse_number<int>(token_buf); \
    if (val.has_value()) { \
      (field) = val.value(); \
    } else { \
      ESP_LOGD(TAG, "invalid " field_name " in line %s", buffer.substr(0, buffer.size() - 2).c_str()); \
      return; \
    } \
  }

#define PARSE_STR(field, field_name) \
  { \
    get_token(field); \
    if (strlen(field) < 2) { \
      ESP_LOGD(TAG, "too short " field_name " in line %s", buffer.substr(0, buffer.size() - 2).c_str()); \
      return; \
    } \
  }

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech";
static const int MAX_DATA_LENGTH_BYTES = 256;
static const uint8_t ASCII_LF = 0x0A;

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  this->check_uart_settings(115200, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "pylontech:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with pylontech failed!");
  }

  for (PylontechListener *listener : this->listeners_) {
    listener->dump_config();
  }

  LOG_UPDATE_INTERVAL(this);
}

void PylontechComponent::setup() {
  while (this->available() != 0) {
    this->read();
  }
}

void PylontechComponent::update() { this->write_str("pwr\n"); }

void PylontechComponent::loop() {
  if (this->available() > 0) {
    // pylontech sends a lot of data very suddenly
    // we need to quickly put it all into our own buffer, otherwise the uart's buffer will overflow
    uint8_t data;
    int recv = 0;
    while (this->available() > 0) {
      if (this->read_byte(&data)) {
        buffer_[buffer_index_write_] += (char) data;
        recv++;
        if (buffer_[buffer_index_write_].back() == static_cast<char>(ASCII_LF) ||
            buffer_[buffer_index_write_].length() >= MAX_DATA_LENGTH_BYTES) {
          // complete line received
          buffer_index_write_ = (buffer_index_write_ + 1) % NUM_BUFFERS;
        }
      }
    }
    ESP_LOGV(TAG, "received %d bytes", recv);
  } else {
    // only process one line per call of loop() to not block esphome for too long
    if (buffer_index_read_ != buffer_index_write_) {
      this->process_line_(buffer_[buffer_index_read_]);
      buffer_[buffer_index_read_].clear();
      buffer_index_read_ = (buffer_index_read_ + 1) % NUM_BUFFERS;
    }
  }
}

void PylontechComponent::process_line_(std::string &buffer) {
  ESP_LOGV(TAG, "Read from serial: %s", buffer.substr(0, buffer.size() - 2).c_str());
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

  const char *cursor = buffer.c_str();
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
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r') {
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
    } else if (strcmp(token_buf, "Power") == 0) {
      // header line i.e. "Power Volt   Curr" and so on
      this->has_tlow_id_ = buffer.find("Tlow.Id") != std::string::npos;
      ESP_LOGD(TAG, "header line %s Tlow.Id: %s", this->has_tlow_id_ ? "with" : "without",
               buffer.substr(0, buffer.size() - 2).c_str());
      return;
    } else {
      ESP_LOGD(TAG, "unknown line %s", buffer.substr(0, buffer.size() - 2).c_str());
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
      ESP_LOGD(TAG, "invalid Coulomb in line %s", buffer.substr(0, buffer.size() - 2).c_str());
      return;
    }
  }
  get_token(token_buf);  // Skip Date
  get_token(token_buf);  // Skip Time
  get_token(token_buf);  // Skip B.V.St
  get_token(token_buf);  // Skip B.T.St
  PARSE_INT(l.mostempr, "Mostempr");

  ESP_LOGD(TAG, "successful line %s", buffer.substr(0, buffer.size() - 2).c_str());

  for (PylontechListener *listener : this->listeners_) {
    listener->on_line_read(&l);
  }
}

}  // namespace pylontech
}  // namespace esphome

#undef PARSE_INT
#undef PARSE_STR
