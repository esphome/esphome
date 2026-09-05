#pragma once

#include "esphome/core/defines.h"

// This component is ESP32-P4 silicon (MIPI-CSI, ISP, hardware JPEG) and builds
// only against esp_video's V4L2 headers, so it compiles on that variant alone.
// A UVC-only configuration leaves i2c_id: out, so there is no I2C component in
// the build and no bus to fetch a handle from.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4) && defined(USE_I2C)

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"

#include "driver/i2c_master.h"

namespace esphome::esp_video_camera {

/// Retrieve the ESP-IDF i2c_master bus handle backing an ESPHome I2CBus.
///
/// The ESPHome I2C bus is already created with the new `i2c_master` driver, so
/// the handle is fetched from the port that bus reports. The configured bus is
/// honoured, instead of returning whichever bus happens to sit on port 0.
inline i2c_master_bus_handle_t get_i2c_bus_handle(i2c::InternalI2CBus *bus) {
  if (bus == nullptr)
    return nullptr;
  i2c_master_bus_handle_t handle = nullptr;
  if (i2c_master_get_bus_handle((i2c_port_num_t) bus->get_port(), &handle) != ESP_OK)
    return nullptr;
  return handle;
}

}  // namespace esphome::esp_video_camera

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4 && USE_I2C
