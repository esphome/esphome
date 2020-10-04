#include "sonoff_sc.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace sonoff_sc {

static const char *TAG = "sonoff_sc";

void SonoffSCComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->raw_data_index_ = 0;
  }

  if (this->available() == 0)
    return;

  this->last_transmission_ = now;
  while (this->available() != 0) {
    uint8_t byte = this->read();
    if (byte == 0x1b) {
      this->parse_data_();
      this->status_clear_warning();
      this->raw_data_index_ = 0;
    } else {
      this->raw_data_[this->raw_data_index_++] = byte;
      if (this->raw_data_index_ > 128) {
        this->status_set_warning();
        this->raw_data_index_ = 0;
      }
    }
  }
}
float SonoffSCComponent::get_setup_priority() const { return setup_priority::DATA; }

int SonoffSCComponent::get_value_for_(const std::string &command, const std::string &prefix) {
  auto start_offset = command.find(prefix);
  if (start_offset == std::string::npos)
    return -1;

  start_offset += prefix.length();

  auto end_offset = command.find(',', start_offset);
  if (end_offset == std::string::npos)
    end_offset = command.length();

  auto value_str = command.substr(start_offset, end_offset - start_offset);
  return strtol(value_str.c_str(), nullptr, 10);
}

void SonoffSCComponent::parse_data_() {
  auto command = std::string((char *) &this->raw_data_, (size_t) this->raw_data_index_);
  ESP_LOGVV(TAG, "Sonoff SC Data: %s", command.c_str());

  if (command == "AT+STATUS?") {
    this->process_status_request_();
  } else if (command.find("AT+UPDATE=") == 0) {
    auto humidity = this->get_value_for_(command, "\"humidity\":");
    auto temperature = this->get_value_for_(command, "\"temperature\":");
    auto light = this->get_value_for_(command, "\"light\":");
    auto noise = this->get_value_for_(command, "\"noise\":");
    auto dust = this->get_value_for_(command, "\"dusty\":");

    if (humidity != -1 && this->humidity_sensor_ != nullptr) {
      this->humidity_sensor_->publish_state(humidity);
    }

    if (temperature != -1 && this->temperature_sensor_ != nullptr) {
      this->temperature_sensor_->publish_state(temperature);
    }

    if (light != -1 && this->light_sensor_ != nullptr) {
      this->light_sensor_->publish_state((int8_t)((10 - light) / 9.0F * 100));
    }

    if (noise != -1 && this->noise_sensor_ != nullptr) {
      this->noise_sensor_->publish_state((noise - 1) / 9.0F * 100);
    }

    if (dust != -1 && this->dust_sensor_ != nullptr) {
      this->dust_sensor_->publish_state((int8_t)((10 - dust) / 9.0F * 100));
    }

  } else if (command == "AT+NOTIFY=ok") {
    // not supported
  } else {
    ESP_LOGV(TAG, "Unknown command received");
  }
}

void SonoffSCComponent::process_status_request_() {
  this->write_str("AT+STATUS\x1b");
  const uint32_t now = millis();

  if (now - this->last_update_time_ > 2 * this->update_interval_sec_) {
    ESP_LOGV(TAG, "Sending devconfig");
    this->write_str(("AT+DEVCONFIG=\"uploadFreq\":" + to_string(this->update_interval_sec_) +
                     ",\"humiThreshold\":" + to_string(this->humidity_threshold_) +
                     ",\"tempThreshold\":" + to_string(this->temperature_threshold_) + "\x1b")
                        .c_str());
  }
}

void SonoffSCComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Sonoff SC:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %u sec", this->update_interval_sec_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_)
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_)
  LOG_SENSOR("  ", "Light", this->light_sensor_)
  LOG_SENSOR("  ", "Noise", this->noise_sensor_)
  LOG_SENSOR("  ", "Dust", this->dust_sensor_)
  this->check_uart_settings(19200);
}

}  // namespace sonoff_sc
}  // namespace esphome
