#include "pylontech.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Helper macros for token-based parsing
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

namespace esphome::pylontech {

static const char *const TAG = "pylontech";
static const int MAX_DATA_LENGTH_BYTES = 256;
static const uint8_t ASCII_LF = 0x0A;

// Helper lambda factory for whitespace-delimited token extraction.
// The returned lambda advances `cursor` past whitespace, copies the next token
// into `dest` (up to TEXT_SENSOR_MAX_LEN-1 chars), and null-terminates it.
static auto make_tokenizer(const char *&cursor) {
  return [&cursor](char *dest) -> void {
    while (*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if (*cursor == '\0') {
      dest[0] = 0;
      return;
    }
    const char *start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r') {
      cursor++;
    }
    size_t token_len = std::min(static_cast<size_t>(cursor - start), static_cast<size_t>(TEXT_SENSOR_MAX_LEN - 1));
    memcpy(dest, start, token_len);
    dest[token_len] = 0;
  };
}

void PylontechListener::on_line_read(LineContents *line) {}
void PylontechListener::on_cell_line_read(CellLineContents *line) {}
void PylontechListener::dump_config() {}

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  this->check_uart_settings(115200, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "pylontech:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with pylontech failed!");
  }

  if (!this->bat_batteries_.empty()) {
    ESP_LOGCONFIG(TAG, "  Cell data requested for batteries:");
    for (int b : this->bat_batteries_) {
      ESP_LOGCONFIG(TAG, "    Battery %d", b);
    }
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

void PylontechComponent::update() {
  this->write_str("pwr\n");
  this->state_ = State::PWR_SENT;
  this->current_bat_index_ = 0;
  this->send_next_bat_ = false;
}

void PylontechComponent::loop() {
  size_t avail = this->available();
  if (avail > 0) {
    // pylontech sends a lot of data very suddenly
    // we need to quickly put it all into our own buffer, otherwise the uart's buffer will overflow
    int recv = 0;
    uint8_t buf[64];
    while (avail > 0) {
      size_t to_read = std::min(avail, sizeof(buf));
      if (!this->read_array(buf, to_read)) {
        break;
      }
      avail -= to_read;
      recv += to_read;

      for (size_t i = 0; i < to_read; i++) {
        buffer_[buffer_index_write_] += (char) buf[i];
        if (buf[i] == ASCII_LF || buffer_[buffer_index_write_].length() >= MAX_DATA_LENGTH_BYTES) {
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

void PylontechComponent::send_bat_command_() {
  if (this->current_bat_index_ < static_cast<int>(this->bat_batteries_.size())) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "bat %d\n", this->bat_batteries_[this->current_bat_index_]);
    this->write_str(cmd);
    this->state_ = State::BAT_SENT;
    ESP_LOGD(TAG, "Sent: bat %d", this->bat_batteries_[this->current_bat_index_]);
  } else {
    this->state_ = State::IDLE;
  }
}

void PylontechComponent::process_line_(std::string &buffer) {
  ESP_LOGV(TAG, "Read from serial: %s", buffer.substr(0, buffer.size() - 2).c_str());

  // Check for end-of-response marker "$$"
  if (buffer.find("$$") != std::string::npos) {
    if (this->state_ == State::PWR_SENT) {
      if (this->cell_polling_enabled_ && !this->bat_batteries_.empty()) {
        this->current_bat_index_ = 0;
        this->send_next_bat_ = true;
      } else {
        this->state_ = State::IDLE;
      }
    } else if (this->state_ == State::BAT_SENT) {
      this->current_bat_index_++;
      if (this->current_bat_index_ < static_cast<int>(this->bat_batteries_.size())) {
        this->send_next_bat_ = true;
      } else {
        this->state_ = State::IDLE;
      }
    }
    return;
  }

  if (this->state_ == State::PWR_SENT) {
    this->parse_pwr_line_(buffer);
  } else if (this->state_ == State::BAT_SENT) {
    this->parse_cell_line_(buffer);
  }
}

void PylontechComponent::parse_pwr_line_(std::string &buffer) {
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
  auto get_token = make_tokenizer(cursor);

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
  {
    get_token(token_buf);
    if (token_buf[0] != '-' || token_buf[1] != '\0') {
      auto val = parse_number<int>(token_buf);
      if (val.has_value()) {
        l.mostempr = val.value();
      }
    }
    // MosTempr may be "-" for batteries without MOS temperature sensor
  }

  ESP_LOGD(TAG, "successful line %s", buffer.substr(0, buffer.size() - 2).c_str());

  for (PylontechListener *listener : this->listeners_) {
    listener->on_line_read(&l);
  }
}

void PylontechComponent::parse_cell_line_(std::string &buffer) {
  // clang-format off
  // example lines to parse (output of "bat N" command):
  // Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  SOC          Coulomb      BAL
  // 0        3308     -914     18300    Dischg       Normal       Normal       Normal       70%         34015 mAH      N
  // clang-format on

  PylontechListener::CellLineContents c{};
  c.bat_num = (this->current_bat_index_ < static_cast<int>(this->bat_batteries_.size()))
                  ? this->bat_batteries_[this->current_bat_index_]
                  : 0;

  const char *cursor = buffer.c_str();
  char token_buf[TEXT_SENSOR_MAX_LEN] = {0};
  auto get_token = make_tokenizer(cursor);

  // Cell number
  {
    get_token(token_buf);
    auto val = parse_number<int>(token_buf);
    if (!val.has_value() || val.value() < 0) {
      // header line or non-data line
      return;
    }
    c.cell_num = val.value();
  }

  PARSE_INT(c.volt, "Volt");
  PARSE_INT(c.curr, "Curr");
  PARSE_INT(c.tempr, "Tempr");

  get_token(token_buf);  // Skip Base State
  get_token(token_buf);  // Skip Volt. State
  get_token(token_buf);  // Skip Curr. State
  get_token(token_buf);  // Skip Temp. State

  // SOC (with % suffix)
  {
    get_token(token_buf);
    for (char &i : token_buf) {
      if (i == '%') {
        i = 0;
        break;
      }
    }
    auto soc_val = parse_number<int>(token_buf);
    if (soc_val.has_value()) {
      c.soc = soc_val.value();
    } else {
      ESP_LOGD(TAG, "invalid SOC in bat line %s", buffer.substr(0, buffer.size() - 2).c_str());
      return;
    }
  }

  PARSE_INT(c.coulomb, "Coulomb");

  get_token(token_buf);  // Skip "mAH"

  // Balancing flag
  {
    get_token(token_buf);
    c.balancing = (token_buf[0] == 'Y');
  }

  ESP_LOGD(TAG, "bat %d cell %d: %dmV %dmA %d.%03d°C %d%% %dmAH bal=%c", c.bat_num, c.cell_num, c.volt, c.curr,
           c.tempr / 1000, c.tempr % 1000, c.soc, c.coulomb, c.balancing ? 'Y' : 'N');

  for (PylontechListener *listener : this->listeners_) {
    listener->on_cell_line_read(&c);
  }
}

}  // namespace esphome::pylontech

#undef PARSE_INT
#undef PARSE_STR
