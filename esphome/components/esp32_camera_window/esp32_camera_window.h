#pragma once

#include "esphome/core/defines.h"

// defines.h sets USE_ESP32_CAMERA_WINDOW for every target, but esp_camera.h only exists in
// ESP-IDF builds, so gate on that too.
#if defined(USE_ESP32_CAMERA_WINDOW) && defined(USE_ESP_IDF)

#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/core/component.h"

#include "esp_camera.h"

namespace esphome::esp32_camera_window {

/// Limits the readout of an ESP32 camera sensor to a rectangular part of the picture.
///
/// Only the OV2640, OV3660, OV5640, SC101IOT and SC030IOT take a window; how each one maps a
/// window onto the published frame is in the component documentation.
class Esp32CameraWindow : public Component {
 public:
  /// A rectangle in published-frame pixels, with (0, 0) at the top left.
  struct Window {
    int offset_x;
    int offset_y;
    int width;
    int height;
  };

  explicit Esp32CameraWindow(esp32_camera::ESP32Camera *camera) : camera_(camera) {}

  /// Runs after the camera's setup(), which initializes the sensor.
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;

  /// Read out the given rectangle of the frame, replacing any previous window.
  ///
  /// The window is clamped to the frame. Returns false when the sensor takes no window or
  /// rejects the settings.
  bool set_window(int offset_x, int offset_y, int width, int height);

  /// Read out the whole sensor again; changing the frame size does the same.
  bool reset_window();

  /// True when the sensor takes a window in the configured pixel format; meant for lambdas.
  bool supports_window() const;

  /// Window to apply in setup(), in frame pixels. No window is applied by default.
  void set_initial_window(int offset_x, int offset_y, int width, int height) {
    this->initial_window_ = {offset_x, offset_y, width, height};
    this->has_initial_window_ = true;
  }

 protected:
  bool set_sensor_window_(sensor_t *sensor, const Window &window);
  sensor_t *checked_sensor_(const char *op) const;
  static bool is_window_supported(sensor_t *sensor);
  static const char *sensor_name(sensor_t *sensor);

  esp32_camera::ESP32Camera *camera_;
  Window initial_window_{0, 0, 0, 0};
  bool has_initial_window_{false};
};

}  // namespace esphome::esp32_camera_window

#endif  // USE_ESP32_CAMERA_WINDOW && USE_ESP_IDF
