#pragma once
#include "hydreon_rgxx.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hydreon_rgxx {

static const char *const TAG = "hydreon_rgxx.sensor";
static const int MAX_DATA_LENGTH_BYTES = 80;
static const uint8_t ASCII_LF = 0x0A;

template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::dump_config() {
  this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "hydreon_rgxx:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with hydreon_rgxx failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  for (int i = 0; i < num_sensors_; i++) {
    LOG_SENSOR("  ", this->sensors_names_[i].c_str(), this->sensors_[i]);
  }
}

template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::setup() {
  ESP_LOGCONFIG(TAG, "Setting up hydreon_rgxx...");
  while (this->available() != 0) {
    this->read();
  }
  this->schedule_reboot();
}

template<size_t num_sensors_> bool HydreonRGxxComponent<num_sensors_>::sensor_missing() {
  if (this->sensors_received_ == -1) {
    // no request sent yet, don't check
    return false;
  } else {
    if (this->sensors_received_ == 0) {
      ESP_LOGW(TAG, "No data at all");
      return true;
    }
    for (int i = 0; i < num_sensors_; i++) {
      if ((this->sensors_received_ >> i & 1) == 0) {
        ESP_LOGW(TAG, "Missing %s", this->sensors_names_[i].c_str());
        return true;
      }
    }
    return false;
  }
}

template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::update() {
  if (this->boot_count_ > 0) {
    if (this->sensor_missing()) {
      this->no_response_count_++;
      ESP_LOGE(TAG, "data missing %d times", this->no_response_count_);
      if (this->no_response_count_ > 15) {
        ESP_LOGE(TAG, "asking sensor to reboot");
        for (int i = 0; i < num_sensors_; i++) {
          this->sensors_[i]->publish_state(NAN);
        }
        this->schedule_reboot();
        return;
      }
    } else {
      this->no_response_count_ = 0;
    }
    this->write_str("R\n");
    this->sensors_received_ = 0;
  }
}

template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_ += (char) data;
      if (this->buffer_.back() == static_cast<char>(ASCII_LF) || this->buffer_.length() >= MAX_DATA_LENGTH_BYTES) {
        // complete line received
        this->process_line();
        this->buffer_.clear();
      }
    }
  }
}

/**
 * Communication with the sensor is asynchronous.
 * We send requests and let esphome continue doing its thing.
 * Once we have received a complete line, we process it.
 *
 * To catch communication failures, we need a timeout.
 * This method implements that. Whenever we expect an answer,
 * this method is called. When an answer is received, the timeout gets cancelled.
 *
 * If no answer is received, the timeout expires and we try to recover the
 * connection by rebooting the sensor. If that fails as well we give up.
 */
template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::schedule_reboot() {
  this->boot_count_ = 0;
  this->set_interval("reboot", 5000, [this]() {
    if (this->boot_count_ < 0) {
      ESP_LOGW(TAG, "hydreon_rgxx failed to boot %d times", -this->boot_count_);
    }
    this->boot_count_--;
    this->write_str("K\n");
    if (this->boot_count_ < -5) {
      ESP_LOGE(TAG, "hydreon_rgxx can't boot, giving up");
      for (int i = 0; i < num_sensors_; i++) {
        this->sensors_[i]->publish_state(NAN);
      }
      this->mark_failed();
    }
  });
}

template<size_t num_sensors_> bool HydreonRGxxComponent<num_sensors_>::buffer_starts_with(const std::string &prefix) {
  return buffer_starts_with(prefix.c_str());
}

template<size_t num_sensors_> bool HydreonRGxxComponent<num_sensors_>::buffer_starts_with(const char *prefix) {
  return buffer_.rfind(prefix, 0) == 0;
}

template<size_t num_sensors_> void HydreonRGxxComponent<num_sensors_>::process_line() {
  ESP_LOGV(TAG, "Read from serial: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());

  if (buffer_[0] == ';') {
    ESP_LOGI(TAG, "Comment: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    return;
  }
  if (buffer_starts_with("PwrDays")) {
    if (this->boot_count_ <= 0) {
      this->boot_count_ = 1;
    } else {
      this->boot_count_++;
    }
    this->cancel_interval("reboot");
    this->no_response_count_ = 0;
    ESP_LOGI(TAG, "Boot detected: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    this->write_str("P\nH\nM\n");  // set sensor to polling mode, high res mode, metric mode
    return;
  }
  if (buffer_starts_with("SW")) {
    std::string::size_type majend = this->buffer_.find(".");
    std::string::size_type endversion = this->buffer_.find(" ", 3);
    if (majend == std::string::npos || endversion == std::string::npos || majend > endversion) {
      ESP_LOGW(TAG, "invalid version string: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    }
    int major = strtol(this->buffer_.substr(3, majend - 3).c_str(), NULL, 10);
    int minor = strtol(this->buffer_.substr(majend + 1, endversion - (majend + 1)).c_str(), NULL, 10);

    if (major > 10 || minor >= 1000 || minor < 0 || major < 0) {
      ESP_LOGW(TAG, "invalid version: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    }
    this->sw_version_ = major * 1000 + minor;
    ESP_LOGI(TAG, "detected sw version %i", this->sw_version_);
    return;
  }
  bool is_data_line = false;
  for (int i = 0; i < num_sensors_; i++) {
    if (buffer_starts_with(this->sensors_names_[i])) {
      is_data_line = true;
      break;
    }
  }
  if (is_data_line) {
    for (int i = 0; i < num_sensors_; i++) {
      std::string::size_type n = this->buffer_.find(this->sensors_names_[i]);
      if (n == std::string::npos) {
        continue;
      }
      int data = strtol(this->buffer_.substr(n + strlen(this->sensors_names_[i].c_str())).c_str(), nullptr, 10);
      // todo: parse TooCold
      this->sensors_[i]->publish_state(data);
      ESP_LOGD(TAG, "Received %s: %f", this->sensors_names_[i].c_str(), this->sensors_[i]->get_raw_state());
      this->sensors_received_ |= (1 << i);
    }
  } else {
    ESP_LOGI(TAG, "Got unknown line: %s", this->buffer_.c_str());
  }
}

template<size_t num_sensors_> float HydreonRGxxComponent<num_sensors_>::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace hydreon_rgxx
}  // namespace esphome
