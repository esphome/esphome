#include "pylontech.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <algorithm>

// Original ESPHome macros for the PWR command parsing
#define PARSE_INT(field, field_name) \
  { \
    get_token(token_buf); \
    auto val = parse_number<int>(token_buf); \
    if (val.has_value()) { \
      (field) = val.value(); \
    } else { \
      return; \
    } \
  }

#define PARSE_STR(field, field_name) \
  { \
    get_token(field); \
    if (strlen(field) < 2) { \
      return; \
    } \
  }

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech";

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  this->check_uart_settings(115200, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "Pylontech Component (PWR + BAT)");
}

void PylontechComponent::setup() {
  while (this->available() != 0) {
    this->read();
  }
}

void PylontechComponent::update() {
  if (this->pylon_state_ != PYLON_IDLE) {
    ESP_LOGW(TAG, "Communication timeout! Serial connection hung in state %d. Resetting state machine...",
             this->pylon_state_);
    this->rx_buffer_.clear();
  }

  this->pylon_state_ = PYLON_SEARCH;
  ESP_LOGD(TAG, "Starting query: Global data (PWR) + %d batteries (BAT)...", this->max_batteries_);
}

void PylontechComponent::loop() {
  if (this->pylon_state_ == PYLON_SEARCH) {
    this->write_str("\n");
    this->pylon_state_ = PYLON_WAIT_WAKEUP;
    return;
  }

  if (this->pylon_state_ == PYLON_REQUEST_PWR) {
    this->write_str("pwr\n");
    this->pylon_state_ = PYLON_READ_PWR;
    return;
  }

  if (this->pylon_state_ == PYLON_REQUEST_BAT) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "bat %d\n", this->current_bat_num_);
    this->write_str(cmd);
    this->pylon_state_ = PYLON_READ_BAT;
    return;
  }

  if (this->pylon_state_ == PYLON_WAIT_WAKEUP || this->pylon_state_ == PYLON_READ_PWR ||
      this->pylon_state_ == PYLON_READ_BAT) {
    while (this->available() > 0) {
      uint8_t c;
      this->read_byte(&c);

      this->rx_buffer_ += (char) c;

      if (this->rx_buffer_.length() > 512 && c != '\n' && c != '>') {
        this->rx_buffer_.clear();
      }

      if (c == '\n' || c == '>') {
        std::string line = this->rx_buffer_;
        this->rx_buffer_.clear();
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

        if (line.find("pylon>") != std::string::npos || line == ">") {
          if (this->pylon_state_ == PYLON_WAIT_WAKEUP) {
            this->pylon_state_ = PYLON_DELAY;
            this->set_timeout("delay", 250, [this]() { this->pylon_state_ = PYLON_REQUEST_PWR; });

          } else if (this->pylon_state_ == PYLON_READ_PWR) {
            this->current_bat_num_ = 1;
            this->pylon_state_ = PYLON_DELAY;
            this->set_timeout("delay", 150, [this]() { this->pylon_state_ = PYLON_REQUEST_BAT; });

          } else if (this->pylon_state_ == PYLON_READ_BAT) {
            if (this->current_bat_num_ < this->max_batteries_) {
              this->current_bat_num_++;
              this->pylon_state_ = PYLON_DELAY;
              this->set_timeout("delay", 150, [this]() { this->pylon_state_ = PYLON_REQUEST_BAT; });
            } else {
              this->pylon_state_ = PYLON_IDLE;
              ESP_LOGD(TAG, "Read cycle completed (PWR + BAT).");
            }
          }

        } else if (!line.empty()) {
          if (this->pylon_state_ == PYLON_READ_PWR) {
            this->process_pwr_line_(line);
          } else if (this->pylon_state_ == PYLON_READ_BAT) {
            this->process_bat_line_(line);
          }
        }
      }
    }
  }
}

// Parser for the PWR command (global battery values)
void PylontechComponent::process_pwr_line_(std::string &buffer) {
  PylontechListener::LineContents l{};
  const char *cursor = buffer.c_str();
  char token_buf[128] = {0};

  auto get_token = [&](char *token_buf) -> void {
    while (*cursor == ' ' || *cursor == '\t')
      cursor++;
    if (*cursor == '\0') {
      token_buf[0] = 0;
      return;
    }
    const char *start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r')
      cursor++;
    size_t token_len = std::min(static_cast<size_t>(cursor - start), static_cast<size_t>(127));
    memcpy(token_buf, start, token_len);
    token_buf[token_len] = 0;
  };

  {
    get_token(token_buf);
    auto val = parse_number<int>(token_buf);
    if (val.has_value() && val.value() > 0) {
      l.bat_num = val.value();
    } else if (strcmp(token_buf, "Power") == 0) {
      this->has_tlow_id_ = buffer.find("Tlow.Id") != std::string::npos;
      return;
    } else {
      return;
    }
  }

  PARSE_INT(l.volt, "Volt");
  PARSE_INT(l.curr, "Curr");
  PARSE_INT(l.tempr, "Tempr");
  PARSE_INT(l.tlow, "Tlow");
  if (this->has_tlow_id_)
    get_token(token_buf);
  PARSE_INT(l.thigh, "Thigh");
  if (this->has_tlow_id_)
    get_token(token_buf);
  PARSE_INT(l.vlow, "Vlow");
  if (this->has_tlow_id_)
    get_token(token_buf);
  PARSE_INT(l.vhigh, "Vhigh");
  if (this->has_tlow_id_)
    get_token(token_buf);
  PARSE_STR(l.base_st, "Base.St");
  PARSE_STR(l.volt_st, "Volt.St");
  PARSE_STR(l.curr_st, "Curr.St");
  PARSE_STR(l.temp_st, "Temp.St");

  {
    get_token(token_buf);
    for (char &i : token_buf)
      if (i == '%') {
        i = 0;
        break;
      }
    auto coul_val = parse_number<int>(token_buf);
    if (coul_val.has_value())
      l.coulomb = coul_val.value();
    else
      return;
  }

  get_token(token_buf);
  get_token(token_buf);
  get_token(token_buf);
  get_token(token_buf);
  PARSE_INT(l.mostempr, "Mostempr");

  for (PylontechListener *listener : this->listeners_) {
    listener->on_line_read(&l);
  }
}

// Memory-efficient parser for the BAT command (individual cell values)
void PylontechComponent::process_bat_line_(std::string &line) {
  if (line.length() < 10 || line.find("bat") == 0 || line.find("Battery") != std::string::npos ||
      line.find("Command") != std::string::npos) {
    return;
  }

  const char *cursor = line.c_str();
  char token_buf[64];

  auto get_token = [&](char *token_buf) -> bool {
    while (*cursor == ' ' || *cursor == '\t')
      cursor++;
    if (*cursor == '\0') {
      token_buf[0] = 0;
      return false;
    }
    const char *start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r')
      cursor++;
    size_t token_len = std::min(static_cast<size_t>(cursor - start), static_cast<size_t>(63));
    memcpy(token_buf, start, token_len);
    token_buf[token_len] = 0;
    return true;
  };

  PylontechListener::CellContents c{};
  c.battery_id = this->current_bat_num_;

  // 1. Cell Number
  if (!get_token(token_buf))
    return;
  auto opt_cell = parse_number<int>(token_buf);
  if (!opt_cell.has_value())
    return;
  c.cell_id = opt_cell.value();

  // 2. Voltage
  if (!get_token(token_buf))
    return;
  auto opt_volt = parse_number<float>(token_buf);
  if (!opt_volt.has_value())
    return;
  c.voltage = opt_volt.value() / 1000.0f;

  // 3. Current
  if (!get_token(token_buf))
    return;
  auto opt_curr = parse_number<float>(token_buf);
  if (!opt_curr.has_value())
    return;
  c.current = opt_curr.value() / 1000.0f;

  // 4. Temperature
  if (!get_token(token_buf))
    return;
  auto opt_temp = parse_number<float>(token_buf);
  if (!opt_temp.has_value())
    return;
  c.temperature = opt_temp.value() / 1000.0f;

  // 5-8. Skip string states (Base, Volt, Curr, Temp)
  for (int i = 0; i < 4; i++) {
    if (!get_token(token_buf))
      return;
  }

  // 9. SOC
  if (!get_token(token_buf))
    return;
  for (char &i : token_buf)
    if (i == '%') {
      i = 0;
      break;
    }
  auto opt_soc = parse_number<int>(token_buf);
  if (!opt_soc.has_value())
    return;
  c.soc = opt_soc.value();

  // 10. Coulomb
  if (!get_token(token_buf))
    return;
  auto opt_coul = parse_number<int>(token_buf);
  if (!opt_coul.has_value())
    return;
  c.coulomb = opt_coul.value();

  // 11. Skip "mAH" label
  if (!get_token(token_buf))
    return;

  // 12. Balance
  if (!get_token(token_buf))
    return;
  c.balance = token_buf[0];

  ESP_LOGD(TAG, "Bat %d Cell %d: %.3fV, %.3fA, %.1fC", c.battery_id, c.cell_id, c.voltage, c.current, c.temperature);

  for (auto *listener : this->listeners_) {
    listener->on_cell_data(&c);
  }
}

}  // namespace pylontech
}  // namespace esphome

#undef PARSE_INT
#undef PARSE_STR
