#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "driver/i2c_master.h"

namespace esphome::esp_video_camera {

/// Retrieve the ESP-IDF i2c_master bus handle backing an ESPHome I2CBus.
///
/// The ESPHome I2C bus is already created with the new `i2c_master` driver, so
/// the handle can be fetched by port number with the official ESP-IDF API.
inline i2c_master_bus_handle_t get_i2c_bus_handle(i2c::I2CBus *bus) {
  if (bus == nullptr)
    return nullptr;
  for (int port = 0; port < 2; port++) {
    i2c_master_bus_handle_t handle = nullptr;
    if (i2c_master_get_bus_handle((i2c_port_num_t) port, &handle) == ESP_OK && handle != nullptr)
      return handle;
  }
  return nullptr;
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF
