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
/// read out at all.
///
/// The OV3660 and OV5640 scale the part of the picture a window covers back up to the frame, so a
/// window on them also zooms and the camera keeps publishing frames of the size it is configured
/// with. The OV2640 crops a window out of its readout and scales that to the size the window has in
/// the frame, so the published frame takes the size of the window instead. That needs JPEG output:
/// in every other pixel format the driver captures a frame of the configured size, which the window
/// would come up short of, so no window is available on it then. The SC101IOT and
/// SC030IOT read a window out as it is and cannot scale it, so a window on them has to be the size
/// of the frame and only moves which part of the sensor the frame shows. Every other sensor the
/// driver knows, such as the OV7725, is read out through registers of its own that do not take a
/// window, so a window is reported as unsupported and every call to set_window() fails on it.
///
/// The OV2640 can only be told to crop in whole four-pixel steps of its readout, and its JPEG
/// encoder compresses whole 16 by 8 pixel blocks, so a window on it is rounded down to those steps;
/// the frame it publishes is the size of the window in the same steps. With another pixel format a
/// window is only rounded down to whole four-pixel steps.
///
/// Repeated set_window() calls replace the previous window; they never nest inside it. Changing
/// the frame size clears any window back to the whole sensor, so re-apply the window afterwards.
/// There is no settle delay: one or two captures after a change may still show the previous
/// window while the sensor reads the new one out.
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
  };

  explicit Esp32CameraWindow(esp32_camera::ESP32Camera *camera) : camera_(camera) {}

  /// Runs after the camera's setup(), which initializes the sensor at DATA priority.
  float get_setup_priority() const override;

  void setup() override;
  void dump_config() override;

  /// Read out the rectangle at offset_x/offset_y of the given size, in frame pixels.
  ///
  /// A window that reaches past the frame is clamped to it, and so is its size. On the OV3660
  /// and OV5640 the frame size the camera publishes is left as it is; on the OV2640 the published
  /// frame takes the size of the window instead, rounded down to whole 16 by 8 pixel blocks with
  /// JPEG output and to whole four-pixel steps with another pixel format. The camera still reports
  /// the frame size it is configured with, so on the OV2640 the window size only shows in the
  /// dimensions of the JPEG frame itself. Returns false when no sensor is available, when the
  /// sensor cannot take a window, or when it rejects the settings. Calling it again replaces the
  /// previous window.
  ///
  /// The SC101IOT and SC030IOT read a window out as it is and cannot scale it, so the size of a
  /// window on them has to be the frame size and the offsets are counted from the corner of the
  /// whole readout of the sensor.
  bool set_window(int offset_x, int offset_y, int width, int height);

  /// Read out the whole sensor again, without changing the frame size.
  ///
  /// Changing the frame size through the camera does the same on its own.
  bool reset_window();

  /// True when the camera has a sensor that accepts a window in the configured pixel format.
  /// Meant for use from lambdas: the component checks the sensor itself before every write.
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
