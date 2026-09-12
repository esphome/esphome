#ifdef USE_ESP32

#include "ecocomfort2_number.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstring>

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.number";

// --- Threshold Number ---

void Ecocomfort2ThresholdNumber::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Ecocomfort2 Threshold Number (%s):", this->threshold_type_ != nullptr ? this->threshold_type_ : "?");
  LOG_NUMBER("  ", "Threshold", this);
}

void Ecocomfort2ThresholdNumber::control(float value) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot change threshold");
    return;
  }
  if (!this->parent_->has_config_data()) {
    ESP_LOGW(TAG, "Configuration not loaded yet, cannot change threshold");
    return;
  }

  // Read current values and write all three together
  uint8_t humidity = this->parent_->get_humidity_threshold();
  uint8_t luminosity = this->parent_->get_luminosity_threshold();
  uint8_t voc = this->parent_->get_voc_threshold();

  uint8_t new_val = static_cast<uint8_t>(value);

  if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "humidity") == 0) {
    humidity = new_val;
  } else if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "luminosity") == 0) {
    luminosity = new_val;
  } else if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "voc") == 0) {
    voc = new_val;
  }

  this->parent_->write_thresholds(humidity, luminosity, voc);
  this->publish_state(value);
}

void Ecocomfort2ThresholdNumber::on_config() {
  if (!this->parent_->has_config_data()) {
    return;
  }

  float value;
  if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "humidity") == 0) {
    value = this->parent_->get_humidity_threshold();
  } else if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "luminosity") == 0) {
    value = this->parent_->get_luminosity_threshold();
  } else if (this->threshold_type_ != nullptr && std::strcmp(this->threshold_type_, "voc") == 0) {
    value = this->parent_->get_voc_threshold();
  } else {
    return;
  }

  if (!this->has_state() || this->state != value) {
    this->publish_state(value);
  }
}

// --- Offset Number ---

void Ecocomfort2OffsetNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Offset Number (%s):", this->offset_type_ != nullptr ? this->offset_type_ : "?");
  LOG_NUMBER("  ", "Offset", this);
}

void Ecocomfort2OffsetNumber::control(float value) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot change offset");
    return;
  }
  if (!this->parent_->has_advanced_data()) {
    ESP_LOGW(TAG, "Advanced settings not loaded yet, cannot change offset");
    return;
  }

  // Read current values and write both together
  int16_t temp_raw = this->parent_->get_temp_offset_raw();
  int16_t hum_raw = this->parent_->get_humidity_offset_raw();

  int16_t new_raw = static_cast<int16_t>(std::lroundf(value * 100.0f));

  if (this->offset_type_ != nullptr && std::strcmp(this->offset_type_, "temperature") == 0) {
    temp_raw = new_raw;
  } else if (this->offset_type_ != nullptr && std::strcmp(this->offset_type_, "humidity") == 0) {
    hum_raw = new_raw;
  }

  this->parent_->write_offsets(temp_raw, hum_raw);
  this->publish_state(value);
}

void Ecocomfort2OffsetNumber::on_config() {
  if (!this->parent_->has_advanced_data()) {
    return;
  }

  float value;
  if (this->offset_type_ != nullptr && std::strcmp(this->offset_type_, "temperature") == 0) {
    value = this->parent_->get_temp_offset_raw() / 100.0f;
  } else if (this->offset_type_ != nullptr && std::strcmp(this->offset_type_, "humidity") == 0) {
    value = this->parent_->get_humidity_offset_raw() / 100.0f;
  } else {
    return;
  }

  if (!this->has_state() || this->state != value) {
    this->publish_state(value);
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
