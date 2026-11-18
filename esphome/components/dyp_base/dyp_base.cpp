#include "dyp_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::dyp_base {

ESPHOME_LOG_TAG(TAG, "dyp_base.sensor");

static constexpr uint32_t FRAME_TIMEOUT_MS = 50;

void DYPSensor::set_mode(DYPMode mode) { this->mode_ = mode; }
void DYPSensor::set_trigger_pin(GPIOPin *trigger_pin) { this->trigger_pin_ = trigger_pin; }
void DYPSensor::set_blind_zone(float blind_zone_m) { this->blind_zone_mm_ = blind_zone_m * 1000; }
void DYPSensor::set_max_range(float max_range_m) { this->max_range_mm_ = max_range_m * 1000; }
void DYPSensor::set_output_mode(DYPOutputMode output_mode) { this->output_mode_ = output_mode; }
void DYPSensor::set_output_mode_pin(GPIOPin *output_mode_pin) { this->output_mode_pin_ = output_mode_pin; }
void DYPSensor::set_publish_mode(DYPPublishMode publish_mode) { this->publish_mode_ = publish_mode; }

void DYPSensor::setup() {
  if (this->trigger_pin_ != nullptr) {
    this->trigger_pin_->setup();
    this->trigger_pin_->digital_write(true);
  }
  if (this->output_mode_pin_ != nullptr) {
    this->output_mode_pin_->setup();
    // Set pin HIGH for PROCESSED (stable) values, LOW for REALTIME
    this->output_mode_pin_->digital_write(this->output_mode_ == DYPOutputMode::DYP_OUTPUT_MODE_PROCESSED);
  }
}

void DYPSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DYP Ultrasonic Distance Sensor:");
  LOG_SENSOR("  ", "Distance", this);
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                this->mode_ == DYPMode::DYP_MODE_CONTROLLED ? LOG_STR_LITERAL("CONTROLLED") : LOG_STR_LITERAL("AUTO"));
  ESP_LOGCONFIG(TAG, "  Publish Mode: %s",
                this->publish_mode_ == DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL ? LOG_STR_LITERAL("INTERVAL")
                                                                                 : LOG_STR_LITERAL("IMMEDIATE"));
  if (this->mode_ == DYPMode::DYP_MODE_CONTROLLED) {
    if (this->trigger_pin_ != nullptr) {
      LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
    } else {
      ESP_LOGCONFIG(TAG, "  Trigger Pin: Using UART TX Pin");
    }
  }
  if (this->mode_ == DYPMode::DYP_MODE_AUTO && this->output_mode_pin_ != nullptr) {
    LOG_PIN("  Output Mode Pin: ", this->output_mode_pin_);
    ESP_LOGCONFIG(TAG, "  Output Mode: %s",
                  this->output_mode_ == DYPOutputMode::DYP_OUTPUT_MODE_PROCESSED ? LOG_STR_LITERAL("PROCESSED")
                                                                                 : LOG_STR_LITERAL("REALTIME"));
  }
  ESP_LOGCONFIG(TAG, "  Blind Zone: %.2fm", this->blind_zone_mm_ / 1000.0f);
  if (this->max_range_mm_ > 0) {
    ESP_LOGCONFIG(TAG, "  Max Range: %.2fm", this->max_range_mm_ / 1000.0f);
  }
}

void DYPSensor::update() {
  if (this->mode_ == DYPMode::DYP_MODE_CONTROLLED) {
    ESP_LOGV(TAG, "Triggering measurement.");
    this->reader_.reset();
    if (this->trigger_pin_ != nullptr) {
      this->trigger_pin_->digital_write(false);
      delay(2);
      this->trigger_pin_->digital_write(true);
    } else {
      this->write_byte(0x00);
    }
  } else if (this->publish_mode_ == DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL) {
    this->publish_state(this->last_distance_m_);
    this->last_distance_m_ = NAN;
  }
}

void DYPSensor::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->reader_.length() > 0 && now - this->last_rx_time_ > FRAME_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Discarding incomplete frame after timeout");
    this->reader_.reset();
  }
  this->read_available([this, now](uint8_t byte) {
    this->last_rx_time_ = now;
    if (const uint8_t *frame = this->reader_.feed(byte)) {
      this->process_frame_(frame);
    }
  });
}

void DYPSensor::process_frame_(const uint8_t *frame) {
  uint8_t checksum_calculated = sum_bytes(frame, 3);
  if (frame[3] != checksum_calculated) {
    ESP_LOGW(TAG, "Checksum mismatch! Received: 0x%02X, Calculated: 0x%02X", frame[3], checksum_calculated);
    this->reader_.resync();
    return;
  }

  uint16_t distance_mm = encode_uint16(frame[1], frame[2]);
  float distance_m = NAN;

  if (this->blind_zone_mm_ > 0 && distance_mm <= this->blind_zone_mm_) {
    ESP_LOGV(TAG, "Distance is within the blind zone (<= %.2fm). Reporting NAN.", this->blind_zone_mm_ / 1000.0f);
    distance_m = NAN;
  } else if (this->max_range_mm_ > 0 && distance_mm > this->max_range_mm_) {
    ESP_LOGV(TAG, "Distance exceeds the max range (> %.2fm). Reporting NAN.", this->max_range_mm_ / 1000.0f);
    distance_m = NAN;
  } else {
    distance_m = distance_mm / 1000.0f;
    ESP_LOGV(TAG, "Received valid frame. Distance: %u mm (%.3f m)", distance_mm, distance_m);
  }

  if (this->mode_ == DYPMode::DYP_MODE_CONTROLLED ||
      this->publish_mode_ == DYPPublishMode::DYP_PUBLISH_MODE_IMMEDIATE) {
    this->publish_state(distance_m);
  } else {
    this->last_distance_m_ = distance_m;
  }
}

}  // namespace esphome::dyp_base
