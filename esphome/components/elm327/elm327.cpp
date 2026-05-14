#include "elm327.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace esphome::elm327 {

static const char *const TAG = "elm327";

// Generous reset window; ATZ reboots the chip and prints its banner before ">"
static const uint32_t INIT_ATZ_TIMEOUT_MS = 3000;
// OBD2 protocol negotiation on ATSP0 can take ~1s on first connect
static const uint32_t INIT_TIMEOUT_MS = 2000;
// Per-PID budget; ELM327 responds in <200ms on established connections
static const uint32_t QUERY_TIMEOUT_MS = 500;

// OBD2 Mode 01 PIDs
static const uint8_t PID_ENGINE_LOAD = 0x04;
static const uint8_t PID_COOLANT_TEMP = 0x05;
static const uint8_t PID_ENGINE_RPM = 0x0C;
static const uint8_t PID_VEHICLE_SPEED = 0x0D;
static const uint8_t PID_INTAKE_AIR_TEMP = 0x0F;
static const uint8_t PID_MAF_RATE = 0x10;
static const uint8_t PID_THROTTLE = 0x11;
static const uint8_t PID_FUEL_LEVEL = 0x2F;

// Sensor slot ordering — must match the publish_pid_ switch below
static const uint8_t NUM_SENSORS = 9;

void ELM327Component::setup() {
  this->send_cmd_("ATZ");
  this->state_ = State::INIT_ATZ;
  this->cmd_sent_at_ = App.get_loop_component_start_time();
}

void ELM327Component::loop() {
  // Accumulate incoming bytes into rx_buf_ without blocking
  while (this->available()) {
    uint8_t c;
    if (!this->read_byte(&c)) {
      break;
    }

    if (c == '>') {
      while (this->rx_pos_ > 0 &&
             (this->rx_buf_[this->rx_pos_ - 1] == '\r' || this->rx_buf_[this->rx_pos_ - 1] == '\n' ||
              this->rx_buf_[this->rx_pos_ - 1] == ' ')) {
        this->rx_pos_--;
      }
      this->rx_buf_[this->rx_pos_] = '\0';
      this->on_response_();
      return;
    }

    if (this->rx_pos_ == 0 && (c == '\r' || c == '\n')) {
      continue;
    }

    if (this->rx_pos_ < sizeof(this->rx_buf_) - 1) {
      this->rx_buf_[this->rx_pos_++] = static_cast<char>(c);
    }
  }

  // Timeout handling — non-blocking, just checks the elapsed time
  uint32_t now = App.get_loop_component_start_time();
  uint32_t timeout_ms = (this->state_ == State::INIT_ATZ)   ? INIT_ATZ_TIMEOUT_MS
                        : (this->state_ == State::QUERYING) ? QUERY_TIMEOUT_MS
                                                            : INIT_TIMEOUT_MS;

  if (now - this->cmd_sent_at_ > timeout_ms) {
    switch (this->state_) {
      case State::INIT_ATZ:
        ESP_LOGW(TAG, "ATZ timed out — check wiring and baud rate");
        this->mark_failed();
        break;
      case State::INIT_ATE0:
        ESP_LOGW(TAG, "ATE0 timed out");
        this->mark_failed();
        break;
      case State::INIT_ATL0:
      case State::INIT_ATH0:
      case State::INIT_ATAT1:
      case State::INIT_ATSP0:
        // Non-critical init commands: log and continue
        ESP_LOGD(TAG, "Init command timed out, continuing");
        this->on_response_();
        break;
      case State::QUERYING:
        ESP_LOGW(TAG, "Query timed out for sensor %u", this->sensor_index_);
        this->sensor_index_++;
        this->send_next_query_();
        break;
      default:
        break;
    }
  }
}

void ELM327Component::update() {
  if (this->state_ == State::IDLE) {
    this->sensor_index_ = 0;
    this->send_next_query_();
  } else if (this->state_ != State::QUERYING) {
    // Still initialising — defer; will be polled again on next update interval
    ESP_LOGD(TAG, "Not ready yet, deferring update");
  }
}

void ELM327Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ELM327:");
  LOG_SENSOR("  ", "Engine RPM", this->engine_rpm_);
  LOG_SENSOR("  ", "Vehicle Speed", this->vehicle_speed_);
  LOG_SENSOR("  ", "Coolant Temperature", this->coolant_temperature_);
  LOG_SENSOR("  ", "Engine Load", this->engine_load_);
  LOG_SENSOR("  ", "Throttle Position", this->throttle_position_);
  LOG_SENSOR("  ", "Intake Air Temperature", this->intake_air_temperature_);
  LOG_SENSOR("  ", "MAF Rate", this->maf_rate_);
  LOG_SENSOR("  ", "Fuel Level", this->fuel_level_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
}

void ELM327Component::send_cmd_(const char *cmd) {
  while (this->available()) {
    this->read();
  }

  this->rx_pos_ = 0;
  this->write_str(cmd);
  this->write_byte('\r');
  this->flush();
  this->cmd_sent_at_ = App.get_loop_component_start_time();
}

void ELM327Component::on_response_() {
  const char *response = this->rx_buf_;
  ESP_LOGV(TAG, "Response (state %u): %s", static_cast<uint8_t>(this->state_), response);

  switch (this->state_) {
    case State::INIT_ATZ:
      ESP_LOGD(TAG, "ATZ: %s", response);
      this->send_cmd_("ATE0");
      this->state_ = State::INIT_ATE0;
      break;

    case State::INIT_ATE0:
      if (strstr(response, "OK") == nullptr) {
        ESP_LOGW(TAG, "ATE0 failed (%s)", response);
        this->mark_failed();
        return;
      }
      this->send_cmd_("ATL0");
      this->state_ = State::INIT_ATL0;
      break;

    case State::INIT_ATL0:
      // ATH0: headers off — response contains only data bytes, simplifying parsing
      this->send_cmd_("ATH0");
      this->state_ = State::INIT_ATH0;
      break;

    case State::INIT_ATH0:
      this->send_cmd_("ATAT1");
      this->state_ = State::INIT_ATAT1;
      break;

    case State::INIT_ATAT1:
      this->send_cmd_("ATSP0");
      this->state_ = State::INIT_ATSP0;
      break;

    case State::INIT_ATSP0:
      ESP_LOGD(TAG, "ELM327 initialized");
      this->state_ = State::IDLE;
      break;

    case State::QUERYING:
      this->publish_pid_(this->sensor_index_, response);
      this->sensor_index_++;
      this->send_next_query_();
      break;

    default:
      break;
  }
}

void ELM327Component::send_next_query_() {
  // Advance past unconfigured sensor slots
  while (this->sensor_index_ < NUM_SENSORS) {
    bool active = false;
    switch (this->sensor_index_) {
      case 0:
        active = this->engine_rpm_ != nullptr;
        break;
      case 1:
        active = this->vehicle_speed_ != nullptr;
        break;
      case 2:
        active = this->coolant_temperature_ != nullptr;
        break;
      case 3:
        active = this->engine_load_ != nullptr;
        break;
      case 4:
        active = this->throttle_position_ != nullptr;
        break;
      case 5:
        active = this->intake_air_temperature_ != nullptr;
        break;
      case 6:
        active = this->maf_rate_ != nullptr;
        break;
      case 7:
        active = this->fuel_level_ != nullptr;
        break;
      case 8:
        active = this->battery_voltage_ != nullptr;
        break;
      default:
        break;
    }
    if (active) {
      break;
    }
    this->sensor_index_++;
  }

  if (this->sensor_index_ >= NUM_SENSORS) {
    this->state_ = State::IDLE;
    this->status_clear_warning();
    return;
  }

  char cmd[7];
  switch (this->sensor_index_) {
    case 0:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_ENGINE_RPM);
      break;
    case 1:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_VEHICLE_SPEED);
      break;
    case 2:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_COOLANT_TEMP);
      break;
    case 3:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_ENGINE_LOAD);
      break;
    case 4:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_THROTTLE);
      break;
    case 5:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_INTAKE_AIR_TEMP);
      break;
    case 6:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_MAF_RATE);
      break;
    case 7:
      snprintf(cmd, sizeof(cmd), "01%02X", PID_FUEL_LEVEL);
      break;
    case 8:
      snprintf(cmd, sizeof(cmd), "ATRV");
      break;
    default:
      break;
  }

  this->send_cmd_(cmd);
  this->state_ = State::QUERYING;
}

void ELM327Component::publish_pid_(uint8_t sensor_index, const char *response) {
  if (strstr(response, "NO DATA") != nullptr || strstr(response, "ERROR") != nullptr ||
      strstr(response, "UNABLE") != nullptr || strstr(response, "?") != nullptr) {
    ESP_LOGW(TAG, "Sensor %u: %s", sensor_index, response);
    this->status_set_warning();
    return;
  }

  if (sensor_index == 8) {
    // Battery voltage: ATRV returns "12.6V"
    char *endptr;
    float v = strtof(response, &endptr);
    if (endptr != response) {
      this->battery_voltage_->publish_state(v);
    } else {
      ESP_LOGW(TAG, "Failed to parse battery voltage: %s", response);
    }
    return;
  }

  uint8_t data[2];
  uint8_t expected = (sensor_index == 0 || sensor_index == 6) ? 2 : 1;
  if (!parse_response_bytes(response, data, expected)) {
    ESP_LOGW(TAG, "Failed to parse response for sensor %u: %s", sensor_index, response);
    this->status_set_warning();
    return;
  }

  switch (sensor_index) {
    case 0:
      this->engine_rpm_->publish_state(((uint16_t(data[0]) << 8) | data[1]) / 4.0f);
      break;
    case 1:
      this->vehicle_speed_->publish_state(data[0]);
      break;
    case 2:
      this->coolant_temperature_->publish_state(int(data[0]) - 40);
      break;
    case 3:
      this->engine_load_->publish_state(data[0] * 100.0f / 255.0f);
      break;
    case 4:
      this->throttle_position_->publish_state(data[0] * 100.0f / 255.0f);
      break;
    case 5:
      this->intake_air_temperature_->publish_state(int(data[0]) - 40);
      break;
    case 6:
      this->maf_rate_->publish_state(((uint16_t(data[0]) << 8) | data[1]) / 100.0f);
      break;
    case 7:
      this->fuel_level_->publish_state(data[0] * 100.0f / 255.0f);
      break;
    default:
      break;
  }
}

bool ELM327Component::parse_response_bytes(const char *response, uint8_t *data, uint8_t expected_bytes) {
  // ATH0 response: "41 0C 1A F8" or "410C1AF8" — first two bytes are mode echo and PID, skip them.
  const char *p = response;
  uint8_t tokens[8];
  uint8_t token_count = 0;

  while (*p != '\0' && token_count < 8) {
    while (*p == ' ' || *p == '\r' || *p == '\n') {
      p++;
    }
    if (*p == '\0') {
      break;
    }

    if (!isxdigit((unsigned char) p[0]) || !isxdigit((unsigned char) p[1])) {
      break;
    }

    char hex[3] = {p[0], p[1], '\0'};
    tokens[token_count++] = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    p += 2;
  }

  if (token_count < 2 + expected_bytes) {
    return false;
  }

  for (uint8_t i = 0; i < expected_bytes; i++) {
    data[i] = tokens[2 + i];
  }

  return true;
}

}  // namespace esphome::elm327
