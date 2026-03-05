#include "pylontech.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstdio>
#include <algorithm>

#define PARSE_INT(field, field_name) \
  { \
    this->get_token_(cursor, token_buf, sizeof(token_buf)); \
    auto val = parse_number<int>(token_buf); \
    if (val.has_value()) { \
      (field) = val.value(); \
    } else { \
      ESP_LOGD(TAG, "invalid " field_name " in line %s", buffer.c_str()); \
      return; \
    } \
  }

#define PARSE_STR(field, field_name) \
  { \
    this->get_token_(cursor, field, sizeof(field)); \
    if (strlen(field) < 2) { \
      ESP_LOGD(TAG, "invalid " field_name " in line %s", buffer.c_str()); \
      return; \
    } \
  }

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech";

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

void PylontechComponent::update() {
  if (this->pylon_state_ != PYLON_IDLE) {
    ESP_LOGW(TAG, "Communication timeout! Serial connection hung in state %d. Resetting state machine...", this->pylon_state_);
    this->rx_buffer_.clear(); 
  }

  this->pylon_state_ = PYLON_SEARCH;
  ESP_LOGD(TAG, "Starting query: Global data (PWR) + %d batteries (BAT)...", this->max_batteries_);
}

bool PylontechComponent::get_token_(const char *&cursor, char *token_buf, size_t max_len) {
  while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
    cursor++;
  }
  if (*cursor == '\0') {
    token_buf[0] = '\0';
    return false;
  }
  const char *start = cursor;
  while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
    cursor++;
  }
  size_t token_len = std::min(static_cast<size_t>(cursor - start), max_len - 1);
  memcpy(token_buf, start, token_len);
  token_buf[token_len] = '\0';
  return true;
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

  if (this->pylon_state_ == PYLON_WAIT_WAKEUP || this->pylon_state_ == PYLON_READ_PWR || this->pylon_state_ == PYLON_READ_BAT) {
    
    uint32_t loop_start = millis(); 
    
    while (this->available() > 0) {
      if (millis() - loop_start > 15) {
        break; 
      }

      uint8_t c;
      this->read_byte(&c);
      
      this->rx_buffer_ += (char)c;

      if (this->rx_buffer_.length() > PYLONTECH_MAX_RX_BUFFER && c != '\n' && c != '>') {
        this->rx_buffer_.clear();
      }

      if (c == '\n' || c == '>') {
        std::string line = this->rx_buffer_;
        this->rx_buffer_.clear();
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

        if (line.find("pylon>") != std::string::npos || line == ">") {
           
           if (this->pylon_state_ == PYLON_WAIT_WAKEUP) {
             this->pylon_state_ = PYLON_DELAY;
             this->set_timeout("delay", PYLONTECH_CMD_DELAY_MS, [this]() { this->pylon_state_ = PYLON_REQUEST_PWR; });
           
           } else if (this->pylon_state_ == PYLON_READ_PWR) {
             this->current_bat_num_ = 1;
             this->pylon_state_ = PYLON_DELAY;
             this->set_timeout("delay", PYLONTECH_CMD_DELAY_MS, [this]() { this->pylon_state_ = PYLON_REQUEST_BAT; });
           
           } else if (this->pylon_state_ == PYLON_READ_BAT) {
             if (this->current_bat_num_ < this->max_batteries_) {
               this->current_bat_num_++;
               this->pylon_state_ = PYLON_DELAY;
               this->set_timeout("delay", PYLONTECH_CMD_DELAY_MS, [this]() { this->pylon_state_ = PYLON_REQUEST_BAT; });
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

void PylontechComponent::process_pwr_line_(std::string &buffer) {
  if (buffer.find("Absent") != std::string::npos) {
    return;
  }

  ESP_LOGV(TAG, "Read from serial (PWR): %s", buffer.c_str());
  
  PylontechListener::LineContents l{};
  const char *cursor = buffer.c_str();
  char token_buf[PYLONTECH_TOKEN_MAX_LEN] = {0};

  {
    this->get_token_(cursor, token_buf, sizeof(token_buf));
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
  
  // clang-format off
  PARSE_INT(l.volt, "Volt");
  PARSE_INT(l.curr, "Curr");
  PARSE_INT(l.tempr, "Tempr");
  PARSE_INT(l.tlow, "Tlow");
  if (this->has_tlow_id_) {
    this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  }
  PARSE_INT(l.thigh, "Thigh");
  if (this->has_tlow_id_) {
    this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  }
  PARSE_INT(l.vlow, "Vlow");
  if (this->has_tlow_id_) {
    this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  }
  PARSE_INT(l.vhigh, "Vhigh");
  if (this->has_tlow_id_) {
    this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  }
  PARSE_STR(l.base_st, "Base.St");
  PARSE_STR(l.volt_st, "Volt.St");
  PARSE_STR(l.curr_st, "Curr.St");
  PARSE_STR(l.temp_st, "Temp.St");
  // clang-format on
  
  {
    this->get_token_(cursor, token_buf, sizeof(token_buf));
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
      return;
    }
  }
  
  this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  this->get_token_(cursor, token_buf, sizeof(token_buf)); 
  PARSE_INT(l.mostempr, "Mostempr");

  for (PylontechListener *listener : this->listeners_) {
    listener->on_line_read(&l);
  }
}

void PylontechComponent::process_bat_line_(std::string &line) {
  if (line.starts_with("bat") || line.find("Battery") != std::string::npos || line.find("Command") != std::string::npos) {
    return;
  }

  const char *cursor = line.c_str();
  char token_buf[PYLONTECH_TOKEN_MAX_LEN] = {0};

  PylontechListener::CellContents c{};
  c.battery_id = this->current_bat_num_;

  // 1. Cell Number (Pylontech starts at index 0!)
  if (!this->get_token_(cursor, token_buf, sizeof(token_buf))) return;
  auto opt_cell = parse_number<int>(token_buf);
  if (!opt_cell.has_value() || opt_cell.value() < 0) return;
  
  // Shift index from 0-14 to 1-15 for Home Assistant
  c.cell_id = opt_cell.value() + 1;

  // 2. Voltage
  if (!this->get_token_(cursor, token_buf, sizeof(token_buf))) return;
  auto opt_volt = parse_number<float>(token_buf);
  if (!opt_volt.has_value()) return;
  c.voltage = opt_volt.value() / 1000.0f;

  // 3. Current
  if (this->get_token_(cursor, token_buf, sizeof(token_buf))) {
    auto opt_curr = parse_number<float>(token_buf);
    if (opt_curr.has_value()) c.current = opt_curr.value() / 1000.0f;
  }

  // 4. Temperature
  if (this->get_token_(cursor, token_buf, sizeof(token_buf))) {
    auto opt_temp = parse_number<float>(token_buf);
    if (opt_temp.has_value()) c.temperature = opt_temp.value() / 1000.0f;
  }

  // 5-8. Skip string states (Base, Volt, Curr, Temp)
  for (int i = 0; i < 4; i++) {
    this->get_token_(cursor, token_buf, sizeof(token_buf));
  }

  // 9. SOC
  if (this->get_token_(cursor, token_buf, sizeof(token_buf))) {
    for (char &i : token_buf) {
      if (i == '%') { i = 0; break; }
    }
    auto opt_soc = parse_number<int>(token_buf);
    if (opt_soc.has_value()) c.soc = opt_soc.value();
  }

  // 10. Coulomb
  if (this->get_token_(cursor, token_buf, sizeof(token_buf))) {
    auto opt_coul = parse_number<int>(token_buf);
    if (opt_coul.has_value()) c.coulomb = opt_coul.value();
  }

  // 11. Skip "mAH" label
  this->get_token_(cursor, token_buf, sizeof(token_buf));

  // 12. Balance
  if (this->get_token_(cursor, token_buf, sizeof(token_buf))) {
    c.balance = token_buf[0];
  }

  ESP_LOGV(TAG, "Bat %d Cell %d: %.3fV, %.3fA, %.1fC", c.battery_id, c.cell_id, c.voltage, c.current, c.temperature);

  for (auto *listener : this->listeners_) {
    listener->on_cell_data(&c);
  }
}

}  // namespace pylontech
}  // namespace esphome

#undef PARSE_INT
#undef PARSE_STR
