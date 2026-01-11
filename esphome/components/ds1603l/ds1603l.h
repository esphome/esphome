#include "esphome/core/log.h"
#include "ds1603l.h"
#include <algorithm>

namespace esphome {
namespace ds1603l {

static const char *const TAG = "ds1603l.sensor";

void Ds1603l::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ds1603l sensor...");

  // Flush any residual data in the UART buffer
  while (this->available() > 0) {
    this->read();
  }
}

void Ds1603l::update() {
  // Loop does it all
}

void Ds1603l::loop() {
  static bool initialized = false;
  static unsigned long start_time = millis();

  // Ignore invalid data during the first 2 seconds after startup
  if (!initialized && (millis() - start_time < 2000)) {
    while (this->available() > 0) {
      this->read();  // Clear any initial invalid data
    }
    return;
  }
  initialized = true;

  // Process incoming data
  while (this->available() >= 4) {
    // Read 4 bytes of data
    this->read_array(this->rx_buffer_, 4);

    ESP_LOGD(TAG, "Raw Data: %02X %02X %02X %02X", this->rx_buffer_[0], this->rx_buffer_[1], this->rx_buffer_[2], this->rx_buffer_[3]);

    // Verify the header byte
    if (this->rx_buffer_[0] != 0xFF) {
      ESP_LOGW(TAG, "Invalid header received");
      continue;
    }
    // Parse the received data
    this->parse_data_();
  }
}

void Ds1603l::dump_config() {
  ESP_LOGCONFIG(TAG, "ds1603l Sensor:");
  LOG_SENSOR("", "Liquid Level", this);
  LOG_SENSOR("", "Liquid Volume", this);
  if (ds1603l_liquid_level_sensor_) {
    ESP_LOGCONFIG(TAG, " Liquid Level id: %s", ds1603l_liquid_level_sensor_->get_name().c_str()); 
  }
  if (ds1603l_liquid_volume_sensor_) {
    ESP_LOGCONFIG(TAG, " Liquid Volume id: %s", ds1603l_liquid_volume_sensor_->get_name().c_str()); 
  }
}

void Ds1603l::parse_data_() {
  uint8_t header = this->rx_buffer_[0];
  uint8_t data_h = this->rx_buffer_[1];
  uint8_t data_l = this->rx_buffer_[2];
  uint8_t checksum = this->rx_buffer_[3];

  // Validate header
  if (header != 0xFF) {
    ESP_LOGW(TAG, "Invalid header: Received 0x%02X, expected 0xFF", header);
    return;
  }

  // Compute checksum
  uint8_t computed_checksum = (header + data_h + data_l) & 0xFF;

  ESP_LOGD(TAG, "Data: Header=0x%02X, Data_H=0x%02X, Data_L=0x%02X, Checksum=0x%02X", header, data_h, data_l, checksum);
  ESP_LOGD(TAG, "Checksum: Computed=0x%02X, Received=0x%02X", computed_checksum, checksum);

  if (checksum != computed_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: Received 0x%02X, expected 0x%02X", checksum, computed_checksum);
    return;
  }

  // Calculate liquid level directly and clamp
  float raw_level = (data_h << 8) | data_l;
  float ds1603l_liquid_level = std::clamp(raw_level, ds1603l_min_level_, ds1603l_max_level_);

  // Calculate liquid volume directly and clamp
  float ds1603l_liquid_volume = ds1603l_liquid_level * (ds1603l_max_volume_ / ds1603l_max_level_);
  ds1603l_liquid_volume = std::clamp(ds1603l_liquid_volume, ds1603l_min_volume_, ds1603l_max_volume_);

  ESP_LOGI(TAG, "Liquid Level: %f mm", ds1603l_liquid_level);

  ESP_LOGI(TAG, "Liquid Volume: %f gal", ds1603l_liquid_volume);

  // Publish values

  if (ds1603l_liquid_level_sensor_) {
    ds1603l_liquid_level_sensor_->publish_state(ds1603l_liquid_level);
  }
  if (ds1603l_liquid_volume_sensor_) {
    ds1603l_liquid_volume_sensor_->publish_state(ds1603l_liquid_volume);
  }
}

}  // namespace ds1603l
}  // namespace esphome
