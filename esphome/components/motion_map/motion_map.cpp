#include "motion_map.h"

#ifdef USE_ESP_IDF

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace motion_map {

static const char *const TAG = "motion_map";

void MotionMapComponent::setup() {
  // Reserve space for variance window
  this->variance_window_.reserve(this->window_size_);

  // Initialize CSI capture
  this->init_csi_();
}

void MotionMapComponent::loop() {
  // Process new CSI data if available
  if (this->new_csi_data_) {
    this->new_csi_data_ = false;
    this->process_csi_data_();
  }

  // Periodic sensor publishing (every 1 second)
  uint32_t now = millis();
  if (now - this->last_update_time_ >= 1000) {
    this->publish_sensors_();
    this->last_update_time_ = now;
  }
}

void MotionMapComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Motion Map:");
  ESP_LOGCONFIG(TAG, "  Motion Threshold: %.2f\n  Idle Threshold: %.2f\n  Window Size: %u\n  Sensitivity: %.2f",
                this->motion_threshold_, this->idle_threshold_, this->window_size_, this->sensitivity_);
  if (this->mac_address_.has_value()) {
    ESP_LOGCONFIG(TAG, "  MAC Filter: %02X:%02X:%02X:%02X:%02X:%02X", (*this->mac_address_)[0],
                  (*this->mac_address_)[1], (*this->mac_address_)[2], (*this->mac_address_)[3],
                  (*this->mac_address_)[4], (*this->mac_address_)[5]);
  }
  if (!this->csi_initialized_) {
    ESP_LOGW(TAG, "CSI not initialized");
  }
}

void MotionMapComponent::init_csi_() {
  // Configure CSI
  wifi_csi_config_t csi_config = {};
  csi_config.lltf_en = true;           // Enable Long Training Field
  csi_config.htltf_en = true;          // Enable HT Long Training Field
  csi_config.stbc_htltf2_en = false;   // Disable STBC HT-LTF2
  csi_config.ltf_merge_en = true;      // Merge LTF
  csi_config.channel_filter_en = true; // Enable channel filter
  csi_config.manu_scale = false;       // Auto scale
  csi_config.shift = 0;                // No shift

  esp_err_t err = esp_wifi_set_csi_config(&csi_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set CSI config: %s", esp_err_to_name(err));
    return;
  }

  // Register CSI callback
  err = esp_wifi_set_csi_rx_cb(MotionMapComponent::csi_callback_, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set CSI RX callback: %s", esp_err_to_name(err));
    return;
  }

  // Enable CSI
  err = esp_wifi_set_csi(true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable CSI: %s", esp_err_to_name(err));
    return;
  }

  this->csi_initialized_ = true;
  ESP_LOGD(TAG, "CSI initialized");
}

void MotionMapComponent::csi_callback_(void *ctx, wifi_csi_info_t *info) {
  auto *component = static_cast<MotionMapComponent *>(ctx);
  if (component == nullptr || info == nullptr || info->buf == nullptr || info->len == 0) {
    return;
  }

  // Copy CSI data to buffer for processing in main loop
  // This callback runs in WiFi task context
  size_t len = std::min(static_cast<size_t>(info->len), MAX_CSI_LEN);
  memcpy(component->csi_buffer_.data.data(), info->buf, len);
  component->csi_buffer_.len = len;
  memcpy(component->csi_buffer_.mac.data(), info->mac, 6);
  component->csi_buffer_.valid = true;
  component->new_csi_data_ = true;
}

void MotionMapComponent::process_csi_data_() {
  if (!this->csi_buffer_.valid) {
    return;
  }

  // Filter by MAC address if configured
  if (this->mac_address_.has_value()) {
    bool mac_match = true;
    for (size_t i = 0; i < 6; i++) {
      if (this->csi_buffer_.mac[i] != (*this->mac_address_)[i]) {
        mac_match = false;
        break;
      }
    }
    if (!mac_match) {
      return;
    }
  }

  // Extract CSI data from buffer
  const int8_t *csi_data = this->csi_buffer_.data.data();
  size_t csi_len = this->csi_buffer_.len;

  // Calculate variance and amplitude
  float variance = this->calculate_variance_(csi_data, csi_len);
  float amplitude = this->calculate_amplitude_(csi_data, csi_len);

  // Apply sensitivity scaling
  variance *= this->sensitivity_;

  // Update moving window
  this->variance_window_.push_back(variance);
  if (this->variance_window_.size() > this->window_size_) {
    this->variance_window_.erase(this->variance_window_.begin());
  }

  // Store current values
  this->current_variance_ = variance;
  this->current_amplitude_ = amplitude;

  // Update motion state
  this->update_motion_state_(variance);
}

float MotionMapComponent::calculate_variance_(const int8_t *data, size_t len) {
  if (len == 0)
    return 0.0f;

  // Calculate mean
  float sum = 0.0f;
  for (size_t i = 0; i < len; i++) {
    sum += static_cast<float>(data[i]);
  }
  float mean = sum / static_cast<float>(len);

  // Calculate variance
  float variance = 0.0f;
  for (size_t i = 0; i < len; i++) {
    float diff = static_cast<float>(data[i]) - mean;
    variance += diff * diff;
  }
  variance /= static_cast<float>(len);

  return variance;
}

float MotionMapComponent::calculate_amplitude_(const int8_t *data, size_t len) {
  if (len == 0)
    return 0.0f;

  // Calculate RMS amplitude
  float sum_sq = 0.0f;
  for (size_t i = 0; i < len; i++) {
    float val = static_cast<float>(data[i]);
    sum_sq += val * val;
  }
  return sqrtf(sum_sq / static_cast<float>(len));
}

float MotionMapComponent::calculate_entropy_() {
  if (this->variance_window_.empty())
    return 0.0f;

  // Simple entropy calculation using histogram
  const int num_bins = 10;
  std::array<int, num_bins> histogram = {};

  // Find min/max for binning
  float min_val = this->variance_window_[0];
  float max_val = this->variance_window_[0];
  for (float val : this->variance_window_) {
    min_val = std::min(min_val, val);
    max_val = std::max(max_val, val);
  }

  float range = max_val - min_val;
  if (range < 0.0001f)
    return 0.0f;

  // Build histogram
  for (float val : this->variance_window_) {
    int bin = static_cast<int>(((val - min_val) / range) * (num_bins - 1));
    bin = std::max(0, std::min(num_bins - 1, bin));
    histogram[bin]++;
  }

  // Calculate entropy
  float entropy = 0.0f;
  float total = static_cast<float>(this->variance_window_.size());
  for (int count : histogram) {
    if (count > 0) {
      float p = static_cast<float>(count) / total;
      entropy -= p * logf(p);
    }
  }

  return entropy;
}

float MotionMapComponent::calculate_skewness_() {
  if (this->variance_window_.size() < 3)
    return 0.0f;

  // Calculate mean
  float sum = 0.0f;
  for (float val : this->variance_window_) {
    sum += val;
  }
  float mean = sum / static_cast<float>(this->variance_window_.size());

  // Calculate standard deviation and skewness
  float variance = 0.0f;
  float skewness_sum = 0.0f;
  for (float val : this->variance_window_) {
    float diff = val - mean;
    variance += diff * diff;
    skewness_sum += diff * diff * diff;
  }

  float n = static_cast<float>(this->variance_window_.size());
  variance /= n;
  float std_dev = sqrtf(variance);

  if (std_dev < 0.0001f)
    return 0.0f;

  float skewness = (skewness_sum / n) / (std_dev * std_dev * std_dev);
  return skewness;
}

void MotionMapComponent::update_motion_state_(float variance) {
  MotionState new_state = this->current_state_;

  // Simple threshold-based state machine
  if (this->current_state_ == MotionState::IDLE) {
    if (variance > this->motion_threshold_) {
      new_state = MotionState::MOTION;
    }
  } else {  // MOTION
    if (variance < this->idle_threshold_) {
      new_state = MotionState::IDLE;
    }
  }

  // Update state if changed
  if (new_state != this->current_state_) {
    this->current_state_ = new_state;
    ESP_LOGV(TAG, "State: %s", new_state == MotionState::MOTION ? "MOTION" : "IDLE");

    // Publish binary sensor immediately on state change
    if (this->motion_binary_sensor_ != nullptr) {
      this->motion_binary_sensor_->publish_state(new_state == MotionState::MOTION);
    }
  }
}

void MotionMapComponent::publish_sensors_() {
  // Publish variance sensor
  if (this->variance_sensor_ != nullptr) {
    this->variance_sensor_->publish_state(this->current_variance_);
  }

  // Publish amplitude sensor
  if (this->amplitude_sensor_ != nullptr) {
    this->amplitude_sensor_->publish_state(this->current_amplitude_);
  }

  // Publish entropy sensor
  if (this->entropy_sensor_ != nullptr && !this->variance_window_.empty()) {
    float entropy = this->calculate_entropy_();
    this->entropy_sensor_->publish_state(entropy);
  }

  // Publish skewness sensor
  if (this->skewness_sensor_ != nullptr && this->variance_window_.size() >= 3) {
    float skewness = this->calculate_skewness_();
    this->skewness_sensor_->publish_state(skewness);
  }
}

}  // namespace motion_map
}  // namespace esphome

#endif  // USE_ESP_IDF
