#include "dlms_push.h"
#include "dlms_parser.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dlms_push {

static const char *const TAG = "dlms_push";

DlmsPushComponent::DlmsPushComponent() { this->parser_ = std::make_unique<DlmsParser>(); }

void DlmsPushComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DLMS PUSH Component...");

  this->rx_buffer_ = std::make_unique<uint8_t[]>(MAX_RX_BUFFER_SIZE);
  this->rx_buffer_len_ = 0;
}

void DlmsPushComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DLMS PUSH Component:");
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Show Log: %s", this->show_log_ ? "True" : "False");

#ifdef USE_SENSOR
  for (const auto &entry : this->sensors_) {
    LOG_SENSOR("  ", "Numeric Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis.c_str());
  }
#endif
}

void DlmsPushComponent::loop() {
  this->read_rx_buffer_();

  if (this->receiving_ &&
      (App.get_loop_component_start_time() - this->last_rx_char_time_ > this->receive_timeout_ms_)) {
    this->receiving_ = false;
    this->process_frame_();
  }
}

void DlmsPushComponent::read_rx_buffer_() {
  int available = this->available();
  if (available == 0)
    return;

  this->receiving_ = true;
  this->last_rx_char_time_ = App.get_loop_component_start_time();

  while (this->available()) {
    if (this->rx_buffer_len_ >= MAX_RX_BUFFER_SIZE) {
      ESP_LOGW(TAG, "RX Buffer overflow. Frame too large! Truncating.");
      break;
    }

    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->rx_buffer_[this->rx_buffer_len_++] = byte;
    } else {
      ESP_LOGW(TAG, "Failed to read byte from UART.");
      break;
    }
  }
}

void DlmsPushComponent::process_frame_() {
  if (this->rx_buffer_len_ == 0)
    return;

  if (this->show_log_) {
    ESP_LOGD(TAG, "Processing received push data");
    ESP_LOGD(TAG, "Processing PUSH data frame with DLMS parser");
    ESP_LOGD(TAG, "PUSH frame size: %zu bytes", this->rx_buffer_len_);
  }

  auto callback = [this](const char *obis_code, float float_val, bool is_numeric) {
    this->on_data_parsed_(obis_code, float_val, is_numeric);
  };

  size_t parsed_objects = this->parser_->parse(this->rx_buffer_.get(), this->rx_buffer_len_, callback, this->show_log_);

  if (this->show_log_) {
    ESP_LOGD(TAG, "PUSH data parsing complete: %zu objects, bytes consumed %zu/%zu", parsed_objects,
             this->rx_buffer_len_, this->rx_buffer_len_);
  }

  this->rx_buffer_len_ = 0;
}

void DlmsPushComponent::on_data_parsed_(const char *obis_code, float float_val, bool is_numeric) {
  int updated_count = 0;

#ifdef USE_SENSOR
  if (is_numeric) {
    for (const auto &entry : this->sensors_) {
      if (entry.obis == obis_code) {
        if (this->show_log_) {
          ESP_LOGD(TAG, "Found sensor for OBIS code %s: '%s'", obis_code, entry.sensor->get_name().c_str());
          ESP_LOGD(TAG, "Publishing data");
        }
        entry.sensor->publish_state(float_val);
        updated_count++;
      }
    }
  }
#endif

  if (this->show_log_ && updated_count == 0) {
    ESP_LOGV(TAG, "Received OBIS %s, but no sensors are registered for it.", obis_code);
  }
}

#ifdef USE_SENSOR
void DlmsPushComponent::register_sensor(const std::string &obis_code, sensor::Sensor *sensor) {
  this->sensors_.push_back({obis_code, sensor});
}
#endif

}  // namespace dlms_push
}  // namespace esphome
