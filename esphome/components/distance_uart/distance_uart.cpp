#include "distance_uart.h"
#include "esphome/core/log.h"
#include <cmath>  // Required for NAN

namespace esphome {
namespace distance_uart {

static const char *const TAG = "distance_uart.sensor";
static const uint8_t FRAME_START = 0xFF;
static const int FRAME_LENGTH = 4;

void DistanceUARTSensor::set_mode(DistanceUARTMode mode) { this->mode_ = mode; }
void DistanceUARTSensor::set_trigger_pin(GPIOPin *trigger_pin) { this->trigger_pin_ = trigger_pin; }
void DistanceUARTSensor::set_blind_zone(float blind_zone_m) { this->blind_zone_mm_ = blind_zone_m * 1000; }
void DistanceUARTSensor::set_max_range(float max_range_m) { this->max_range_mm_ = max_range_m * 1000; }
void DistanceUARTSensor::set_output_mode(DistanceUARTOutputMode output_mode) { this->output_mode_ = output_mode; }
void DistanceUARTSensor::set_output_mode_pin(GPIOPin *output_mode_pin) { this->output_mode_pin_ = output_mode_pin; }
void DistanceUARTSensor::set_publish_mode(DistanceUARTPublishMode publish_mode) { this->publish_mode_ = publish_mode; }
void DistanceUARTSensor::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }

void DistanceUARTSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Distance UART Sensor...");
  if (this->trigger_pin_ != nullptr) {
    this->trigger_pin_->setup();
    this->trigger_pin_->digital_write(true);
  }
  if (this->output_mode_pin_ != nullptr) {
    this->output_mode_pin_->setup();
    // Set pin HIGH for PROCESSED (stable) values, LOW for REALTIME
    this->output_mode_pin_->digital_write(this->output_mode_ == OUTPUT_MODE_PROCESSED);
  }
}

void DistanceUARTSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Distance UART Sensor:");
  LOG_SENSOR("  ", "Distance", this);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_ == MODE_CONTROLLED ? "CONTROLLED" : "AUTO");
  ESP_LOGCONFIG(TAG, "  Publish Mode: %s", this->publish_mode_ == PUBLISH_MODE_INTERVAL ? "INTERVAL" : "IMMEDIATE");
  if (this->mode_ == MODE_CONTROLLED) {
    if (this->trigger_pin_ != nullptr) {
      LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
    } else {
      ESP_LOGCONFIG(TAG, "  Trigger Pin: Using UART TX Pin");
    }
  }
  if (this->mode_ == MODE_AUTO && this->output_mode_pin_ != nullptr) {
    LOG_PIN("  Output Mode Pin: ", this->output_mode_pin_);
    ESP_LOGCONFIG(TAG, "  Output Mode: %s", this->output_mode_ == OUTPUT_MODE_PROCESSED ? "PROCESSED" : "REALTIME");
  }
  ESP_LOGCONFIG(TAG, "  Blind Zone: %.2fm", this->blind_zone_mm_ / 1000.0f);
  if (this->max_range_mm_ > 0) {
    ESP_LOGCONFIG(TAG, "  Max Range: %.2fm", this->max_range_mm_ / 1000.0f);
  }
  this->check_uart_settings(this->baud_rate_);
}

void DistanceUARTSensor::update() {
  if (this->mode_ == MODE_CONTROLLED) {
    ESP_LOGV(TAG, "Triggering measurement.");
    if (this->trigger_pin_ != nullptr) {
      this->trigger_pin_->digital_write(false);
      delay(2);
      this->trigger_pin_->digital_write(true);
    } else {
      this->write_byte(0x00);
    }
  } else {  // MODE_AUTO
    // Only publish on interval if the publish_mode is set to INTERVAL
    if (this->publish_mode_ == PUBLISH_MODE_INTERVAL) {
      ESP_LOGD(TAG, "Publishing last known distance: %.3f m", this->last_distance_m_);
      this->publish_state(this->last_distance_m_);
      this->last_distance_m_ = NAN;
    }
  }
}

void DistanceUARTSensor::loop() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == 0) {
      if (byte == FRAME_START) {
        this->buffer_[this->read_pos_++] = byte;
      }
    } else {
      this->buffer_[this->read_pos_++] = byte;
      if (this->read_pos_ >= FRAME_LENGTH) {
        this->process_frame_();
        this->read_pos_ = 0;
      }
    }
  }
}

void DistanceUARTSensor::process_frame_() {
  uint8_t start_byte = this->buffer_[0];
  uint8_t data_h = this->buffer_[1];
  uint8_t data_l = this->buffer_[2];
  uint8_t checksum_received = this->buffer_[3];

  if (start_byte != FRAME_START) {
    ESP_LOGW(TAG, "Invalid start byte: 0x%02X", start_byte);
    return;
  }

  uint8_t checksum_calculated = (start_byte + data_h + data_l) & 0xFF;
  if (checksum_received != checksum_calculated) {
    ESP_LOGW(TAG, "Checksum mismatch! Received: 0x%02X, Calculated: 0x%02X", checksum_received, checksum_calculated);
    return;
  }

  uint16_t distance_mm = (data_h << 8) | data_l;
  float distance_m = NAN;

  if (this->blind_zone_mm_ > 0 && distance_mm <= this->blind_zone_mm_) {
    ESP_LOGW(TAG, "Distance is within the blind zone (<= %.2fm). Ignoring reading.", this->blind_zone_mm_ / 1000.0f);
    distance_m = NAN;
  } else if (this->max_range_mm_ > 0 && distance_mm > this->max_range_mm_) {
    ESP_LOGW(TAG, "Distance exceeds the max range (> %.2fm). Ignoring reading.", this->max_range_mm_ / 1000.0f);
    distance_m = NAN;
  } else {
    distance_m = distance_mm / 1000.0f;
    ESP_LOGV(TAG, "Received valid frame. Distance: %u mm (%.3f m)", distance_mm, distance_m);
  }

  if (this->mode_ == MODE_CONTROLLED) {
    this->publish_state(distance_m);
  } else {  // MODE_AUTO
    if (this->publish_mode_ == PUBLISH_MODE_IMMEDIATE) {
      this->publish_state(distance_m);
    } else {  // PUBLISH_MODE_INTERVAL
      this->last_distance_m_ = distance_m;
    }
  }
}

}  // namespace distance_uart
}  // namespace esphome
