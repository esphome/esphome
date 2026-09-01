#include "ds1603l.h"
#include <algorithm>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ds1603l {

static const char *const TAG = "ds1603l.sensor";

void DS1603L::setup() {
  // Flush any residual data in the UART buffer
  while (this->available() > 0) {
    this->read();
  }
}

void DS1603L::loop() {
  // Ignore invalid data during the first 2 seconds after startup
  if (!this->initialized_ && (App.get_loop_component_start_time() < 2000)) {
    while (this->available() > 0) {
      this->read();  // Clear any initial invalid data
    }
    return;
  }
  this->initialized_ = true;

  // Process incoming data
  while (this->available() >= 4) {
    // Read 4 bytes of data
    this->read_array(this->rx_buffer_, 4);

    ESP_LOGV(TAG, "Raw Data: %02X %02X %02X %02X", this->rx_buffer_[0], this->rx_buffer_[1], this->rx_buffer_[2],
             this->rx_buffer_[3]);

    // Verify the header byte
    if (this->rx_buffer_[0] != 0xFF) {
      ESP_LOGW(TAG, "Invalid header received");
      continue;
    }
    // Parse the received data
    this->parse_data_();
  }
}

void DS1603L::dump_config() { LOG_SENSOR("  ", "DS1603L:", this); }

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

  ESP_LOGV(TAG, "Data: Header=0x%02X, Data_H=0x%02X, Data_L=0x%02X, Checksum=0x%02X", header, data_h, data_l, checksum);
  ESP_LOGV(TAG, "Checksum: Computed=0x%02X, Received=0x%02X", computed_checksum, checksum);

  if (checksum != computed_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: Received 0x%02X, expected 0x%02X", checksum, computed_checksum);
    return;
  }

  // Calculate liquid level directly and clamp
  uint16_t level = encode_uint16(data_h, data_l);
  this->publish_state(level);
}

}  // namespace esphome::ds1603l
