#include "esphome/core/log.h"
#include "DS1603L.h"
#include <algorithm>

namespace esphome {
namespace DS1603L {

static const char *TAG = "DS1603L.sensor";

void DS1603L::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS1603L sensor...");

  // Add a delay to allow the sensor to initialize after power-up
  delay(2000); // 2 seconds

  // Flush any residual data in the UART buffer
  while (this->available() > 0) {
    this->read();
  }
}

void DS1603L::update() {
  // Loop does it all
}

void DS1603L::loop() {
  static bool initialized = false;
  static unsigned long start_time = millis();

  // Ignore invalid data during the first 2 seconds after startup
  if (!initialized && (millis() - start_time < 2000)) {
    while (this->available() > 0) {
      this->read(); // Clear any initial invalid data
    }
    return;
  }
  initialized = true;

  // Process incoming data
  while (this->available() >= 4) {
    // Read 4 bytes of data
    this->read_array(this->rx_buffer_, 4);

    ESP_LOGD(TAG, "Raw Data: %02X %02X %02X %02X", 
             this->rx_buffer_[0], this->rx_buffer_[1], 
             this->rx_buffer_[2], this->rx_buffer_[3]);

    // Verify the header byte
    if (this->rx_buffer_[0] != 0xFF) {
      ESP_LOGW(TAG, "Invalid header received");
      continue;
    }
    // Parse the received data
    this->parse_data_();
  }
}

void DS1603L::dump_config() {
  ESP_LOGCONFIG(TAG, "DS1603L Sensor:");
  LOG_SENSOR("", "Liquid Level", this);
  LOG_SENSOR("", "Liquid Volume", this);
  if (liquid_level_sensor_) { 
    ESP_LOGCONFIG(TAG, " Liquid Level id: %s", liquid_level_sensor_->get_name().c_str()); }
  if (liquid_volume_sensor_) { 
    ESP_LOGCONFIG(TAG, " Liquid Volume id: %s", liquid_volume_sensor_->get_name().c_str()); }
}

void DS1603L::parse_data_() {
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

  ESP_LOGD(TAG, "Data: Header=0x%02X, Data_H=0x%02X, Data_L=0x%02X, Checksum=0x%02X", 
           header, data_h, data_l, checksum);
  ESP_LOGD(TAG, "Checksum: Computed=0x%02X, Received=0x%02X", computed_checksum, checksum);

  if (checksum != computed_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: Received 0x%02X, expected 0x%02X", checksum, computed_checksum);
    return;
  }

  // Calculate liquid level directly and clamp
  float raw_level = (data_h << 8) | data_l;
  float liquid_level = std::clamp(raw_level, min_level_, max_level_);
  
  // Calculate liquid volume directly and clamp
  float liquid_volume = liquid_level * (max_volume_/max_level_);
  liquid_volume = std::clamp(liquid_volume, min_volume_, max_volume_);
  
  ESP_LOGI(TAG, "Liquid Level: %f mm", liquid_level);
  
  ESP_LOGI(TAG, "Liquid Volume: %f gal", liquid_volume);

  // Publish values
  
  if (liquid_level_sensor_) { 
    liquid_level_sensor_->publish_state(liquid_level); 
  }
  if (liquid_volume_sensor_) { 
    liquid_volume_sensor_->publish_state(liquid_volume); 
  }
}

}  // namespace DS1603L
}  // namespace esphome
