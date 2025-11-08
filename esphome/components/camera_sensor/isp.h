#pragma once

#if USE_ESP32_VARIANT_ESP32P4 && USE_CSI_CAMERA_SENSOR

#include "esphome/components/camera/camera.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include "driver/isp.h"
#include "esp_cam_sensor.h"

namespace esphome::camera_sensor {

/// Supported Bayer filter patterns for raw camera sensors.
enum BayerPattern : uint8_t {
  AUTO = 0,  ///< Automatically detect the Bayer pattern.
  RGGB,      ///< Red-Green-Green-Blue (standard orientation)
  GRBG,      ///< Green-Red-Blue-Green (RGGB, x flipped)
  GBRG,      ///< Green-Blue-Red-Green (RGGB, y flipped)
  BGGR,      ///< Blue-Green-Green-Red (RGGB, x and y flipped)
};

/// Returns string name for given Bayer filter pattern.
inline const char *to_string(BayerPattern pattern) {
  switch (pattern) {
    case AUTO:
      return "AUTO";
    case RGGB:
      return "RGGB";
    case GRBG:
      return "GRBG";
    case GBRG:
      return "GBRG";
    case BGGR:
      return "BGGR";
  }

  return "AUTO";
}

/// Image Signal Processor (ISP) for CSI camera sensors.
/// This class receives raw data from a CSI camera sensor and converts it to RGB565 or RGB888 format.
/// It also provides common image adjustments such as hue, contrast, brightness, saturation, and
/// basic noise filtering.
class ISP {
 public:
#ifdef USE_NUMBER
  SUB_NUMBER(brightness)
  SUB_NUMBER(contrast)
  SUB_NUMBER(filter)
  SUB_NUMBER(hue)
  SUB_NUMBER(saturation)
  SUB_NUMBER(red)
  SUB_NUMBER(green)
  SUB_NUMBER(blue)
#endif
  /// Set image brightness level.
  void set_brightness(int8_t brightness) { this->brightness_ = brightness; }
  /// Set image contrast level.
  void set_contrast(uint8_t contrast) { this->contrast_ = contrast; }
  /// Set noise filter strength.
  void set_filter(uint8_t filter) { this->filter_ = filter; }
  /// Set image hue adjustment
  void set_hue(uint16_t hue) { this->hue_ = hue; }
  /// Set image saturation level.
  void set_saturation(uint8_t saturation) { this->saturation_ = saturation; }
  /// Set the Bayer pattern used by the camera sensor.
  void set_bayer_pattern(BayerPattern bayer_pattern) { this->bayer_pattern_ = bayer_pattern; }
  // Set the color correction for red.
  void set_red(float red) { this->red_ = red; }
  // Set the color correction for green.
  void set_green(float green) { this->green_ = green; }
  // Set the color correction for blue.
  void set_blue(float blue) { this->blue_ = blue; }
  /// Update brightness from a numeric input entity.
  void number_brightness(float value);
  /// Update contrast from a numeric input entity.
  void number_contrast(float value);
  /// Update filter strength from a numeric input entity.
  void number_filter(float value);
  /// Update hue adjustment from a numeric input entity.
  void number_hue(float value);
  /// Update saturation from a numeric input entity.
  void number_saturation(float value);
  /// Adjust red from a numeric input entity.
  void number_red(float value);
  /// Adjust green from a numeric input entity.
  void number_green(float value);
  /// Adjust blue from a numeric input entity.
  void number_blue(float value);
  /// Configure ISP based on camera format, pixel format, and flip options.
  bool configure_isp(esp_cam_sensor_format_t *format, camera::PixelFormat pixel_format, bool flip_x, bool flip_y);
  /// Log current ISP configuration settings.
  void log_config();

 protected:
  void configure_color_();
  void configure_filter_();
  void configure_color_correction_();
  isp_color_t to_internal_(camera::PixelFormat format);
  isp_color_t to_internal_(esp_cam_sensor_output_format_t format);
  BayerPattern to_internal_(esp_cam_sensor_bayer_pattern_t pattern);
  color_raw_element_order_t bayer_to_isp_(bool flip_x, bool flip_y);

  int8_t brightness_{};
  uint8_t saturation_{};
  uint8_t contrast_{};
  uint8_t filter_{};
  BayerPattern bayer_pattern_{};
  BayerPattern bayer_pattern_reported_{};
  uint16_t hue_{};
  float red_{};
  float green_{};
  float blue_{};
  isp_proc_handle_t isp_proc_handle_{};
};

}  // namespace esphome::camera_sensor

#endif
