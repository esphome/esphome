#pragma once

#include "esphome/core/defines.h"

// The feature define alone is not a gate: esphome/core/defines.h, which the clang-tidy
// header run lints, sets it for every target, while esp_camera.h only exists in ESP-IDF
// builds. The component is ESP-IDF only, so gate on that too.
#if defined(USE_ESP32_CAMERA_WINDOW) && defined(USE_ESP_IDF)

#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/core/component.h"

#include "esp_camera.h"

namespace esphome::esp32_camera_window {

/// Limits the readout of an ESP32 camera sensor to a rectangular part of the picture.
///
/// Only the part of the sensor that is read out changes, so pixels outside the window are never
/// read out at all. The camera keeps publishing frames of the size it is configured with, so
/// nothing else in the configuration changes; the window is filled with the part of the picture
/// that it covers.
///
/// The OV2640, OV3660 and OV5640 scale the part of the picture a window covers back up to the frame,
/// so a window on them also zooms. The SC101IOT and SC030IOT read a window out as it is and cannot
/// scale it, so a window on them has to be the size of the frame and only moves which part of the
/// sensor the frame shows. Every other sensor the driver knows, such as the OV7725, is read out
/// through registers of its own that do not take a window, so a window is reported as unsupported
/// and every call to set_window() fails on it.
///
/// The OV2640 can only be told to crop in whole four-pixel steps of its readout, so a window on it
/// is rounded down to one.
class Esp32CameraWindow : public Component {
 public:
  /// A rectangle of the picture that is read out.
  ///
  /// It is measured in the pixels of the published frame, with (0, 0) at its top left corner, so
  /// the same window asks for the same part of the picture whatever frame size is in use.
  struct Window {
    int offset_x;
    int offset_y;
    int width;
    int height;
    bool enabled;
  };

  explicit Esp32CameraWindow(esp32_camera::ESP32Camera *camera) : camera_(camera) {}

  /// Runs after the camera's setup(), which initializes the sensor at DATA priority.
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;

  /// Read out the rectangle at offset_x/offset_y of the given size, in frame pixels.
  ///
  /// A window that reaches past the frame is clamped to it, and so is its size. The frame size
  /// the camera publishes is left as it is. Returns false when no sensor is available, when the
  /// sensor cannot take a window, or when it rejects the settings.
  ///
  /// The SC101IOT and SC030IOT read a window out as it is and cannot scale it, so the size of a
  /// window on them has to be the frame size and the offsets are counted from the corner of the
  /// whole readout of the sensor.
  bool set_window(int offset_x, int offset_y, int width, int height);

  /// Read out the whole sensor again, without changing the frame size.
  bool reset_window();

  /// True when the camera has a sensor that accepts a window.
  bool supports_window() const;

  /// Window to apply in setup(), in frame pixels. No window is applied by default.
  void set_initial_window(int offset_x, int offset_y, int width, int height) {
    this->initial_window_ = {offset_x, offset_y, width, height, true};
  }

 protected:
  bool set_sensor_window_(sensor_t *sensor, const Window &window);
  static bool is_window_supported_(sensor_t *sensor);
  static const char *sensor_name_(sensor_t *sensor);

  esp32_camera::ESP32Camera *camera_;
  Window initial_window_{0, 0, 0, 0, false};
};

}  // namespace esphome::esp32_camera_window

#endif  // USE_ESP32_CAMERA_WINDOW && USE_ESP_IDF
