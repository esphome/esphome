#include "landisgyr.h"

#include <string>

namespace esphome {
namespace landisgyr {

static char const *const TAG = "UH50";

void LandisSensor::update() {
  // Wait a bit till we have logs
  if (!initialized_) {
    initialized_ = true;
    return;
  }

  buffer_string_.clear();

  if (state_ == State::IDLE) {
    send_request_();
    state_ = State::RECIEVE_INIT;
  } else {
    ESP_LOGE("custom", "Update called but we are not ready yet. something is wrong!");
  }
}

void LandisSensor::loop() {
  if (state_ != State::RECEIVE_VALUES && state_ != State::RECIEVE_INIT)
    return;

  auto line = read_line_();
  if (line.empty())
    return;

  if (state_ == State::RECIEVE_INIT) {
    ESP_LOGD(TAG, "initialization message %s", line.c_str());
    this->parent_->set_baud_rate(2400);
    this->parent_->load_settings(false);
    state_ = State::RECEIVE_VALUES;
  } else if (state_ == State::RECEIVE_VALUES) {
    ESP_LOGD(TAG, "Values: %s", line.c_str());
    if (read_only_first_line_) {
      parse_first_line_(line);
      state_ = State::IDLE;
    } else if (buffer_string_.find('!'))
      state_ = State::IDLE;
  }
}

void LandisSensor ::dump_config() {
  ESP_LOGCONFIG(TAG, "LandisGur UH50");
  LOG_SENSOR(TAG, "Energy KWh", this->kwh_sensor_);
  LOG_SENSOR(TAG, "Water m³", this->kwh_sensor_);
}

void LandisSensor::send_request_() {
  this->parent_->set_baud_rate(300);
  this->parent_->load_settings(false);

  for (int i = 0; i <= 40; i++)
    this->write(0x00);
  this->write_str("\x2F\x3F\x21\x0D\x0A");

  ESP_LOGD(TAG, "init message sent");
}

std::string LandisSensor::read_line_() {
  while (available()) {
    char c = read();
    buffer_string_ += c;
    if (c == '\n') {
      auto r = buffer_string_;
      buffer_string_.clear();
      return r;
    }
  }
  return "";
}

std::string LandisSensor::parse_delimiter_(const std::string &string_to_parse, const std::string &first,
                                           const std::string &second) {
  auto pos1 = string_to_parse.find(first);
  auto pos2 = string_to_parse.find(second);
  if (pos1 == -1 || pos2 == -1) {
    ESP_LOGE(TAG, "failed to parse %s", string_to_parse.c_str());
    return "";
  }

  return string_to_parse.substr(pos1 + first.length(), pos2 - pos1 - first.length());
}

void LandisSensor::parse_first_line_(const std::string &line) {
  auto energy = parse_delimiter_(line, "6.8(", "*MWh");
  auto water = parse_delimiter_(line, "6.26(", "*m3");

  if (kwh_sensor_ != nullptr)
    kwh_sensor_->publish_state(strtof(energy.c_str(), nullptr));
  if (volume_sensor_ != nullptr)
    volume_sensor_->publish_state(strtof(water.c_str(), nullptr));
}

}  // namespace landisgyr
}  // namespace esphome
