#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP_IDF
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <array>
#include <cmath>
#include <vector>

namespace esphome {

// Forward declarations
namespace binary_sensor {
class BinarySensor;
}
namespace sensor {
class Sensor;
}

namespace motion_map {

/// Motion state enumeration
enum class MotionState : uint8_t {
  IDLE = 0,
  MOTION = 1,
};

/// Maximum CSI buffer size for ESP32-S3
static constexpr size_t MAX_CSI_LEN = 384;

/// CSI data buffer for cross-task communication
struct CSIDataBuffer {
  std::array<int8_t, MAX_CSI_LEN> data;
  size_t len{0};
  std::array<uint8_t, 6> mac;
  bool valid{false};
};

/**
 * @brief Motion Map Component using Wi-Fi CSI for motion detection
 *
 * This component captures Channel State Information (CSI) from Wi-Fi packets
 * and analyzes signal variations to detect motion without cameras or microphones.
 */
class MotionMapComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Configuration setters
  void set_motion_threshold(float threshold) { this->motion_threshold_ = threshold; }
  void set_idle_threshold(float threshold) { this->idle_threshold_ = threshold; }
  void set_window_size(uint32_t size) { this->window_size_ = size; }
  void set_sensitivity(float sensitivity) { this->sensitivity_ = sensitivity; }
  void set_mac_address(const std::array<uint8_t, 6> &mac) { this->mac_address_ = mac; }

  // Sensor setters
  void set_motion_binary_sensor(binary_sensor::BinarySensor *sensor) { this->motion_binary_sensor_ = sensor; }
  void set_variance_sensor(sensor::Sensor *sensor) { this->variance_sensor_ = sensor; }
  void set_amplitude_sensor(sensor::Sensor *sensor) { this->amplitude_sensor_ = sensor; }
  void set_entropy_sensor(sensor::Sensor *sensor) { this->entropy_sensor_ = sensor; }
  void set_skewness_sensor(sensor::Sensor *sensor) { this->skewness_sensor_ = sensor; }

 protected:
  /// Initialize CSI capture
  void init_csi_();

  /// CSI callback (static wrapper for ESP-IDF) - runs in WiFi task
  static void csi_callback_(void *ctx, wifi_csi_info_t *info);

  /// Process CSI data in main loop
  void process_csi_data_();

  /// Calculate variance from CSI data
  float calculate_variance_(const int8_t *data, size_t len);

  /// Calculate amplitude from CSI data
  float calculate_amplitude_(const int8_t *data, size_t len);

  /// Calculate entropy from variance window
  float calculate_entropy_();

  /// Calculate skewness from variance window
  float calculate_skewness_();

  /// Update motion state based on current variance
  void update_motion_state_(float variance);

  /// Publish sensor values
  void publish_sensors_();

  // Configuration parameters
  float motion_threshold_{0.5f};
  float idle_threshold_{0.2f};
  uint32_t window_size_{100};
  float sensitivity_{1.0f};
  optional<std::array<uint8_t, 6>> mac_address_;

  // Sensors
  binary_sensor::BinarySensor *motion_binary_sensor_{nullptr};
  sensor::Sensor *variance_sensor_{nullptr};
  sensor::Sensor *amplitude_sensor_{nullptr};
  sensor::Sensor *entropy_sensor_{nullptr};
  sensor::Sensor *skewness_sensor_{nullptr};

  // Runtime state
  MotionState current_state_{MotionState::IDLE};
  std::vector<float> variance_window_;
  float current_variance_{0.0f};
  float current_amplitude_{0.0f};
  uint32_t last_update_time_{0};
  bool csi_initialized_{false};

  // CSI data buffer (written by WiFi task, read by main loop)
  CSIDataBuffer csi_buffer_;
  volatile bool new_csi_data_{false};
};

}  // namespace motion_map
}  // namespace esphome

#endif  // USE_ESP_IDF
