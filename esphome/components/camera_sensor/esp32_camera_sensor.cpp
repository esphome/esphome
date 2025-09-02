#ifdef USE_ESP32_CAMERA_SENSOR

#include "esp32_camera_sensor.h"

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

ESP32CameraSensor::ESP32CameraSensor(camera::CameraImageSpec *spec) { this->image_spec_ = spec; }

void ESP32CameraSensor::set_pins(int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7, int xclk, int vsync,
                                 int href, int pclk, int pwdn, int reset) {
  this->camera_config_.pin_d0 = d0;
  this->camera_config_.pin_d1 = d1;
  this->camera_config_.pin_d2 = d2;
  this->camera_config_.pin_d3 = d3;
  this->camera_config_.pin_d4 = d4;
  this->camera_config_.pin_d5 = d5;
  this->camera_config_.pin_d6 = d6;
  this->camera_config_.pin_d7 = d7;

  this->camera_config_.pin_xclk = xclk;
  this->camera_config_.pin_vsync = vsync;
  this->camera_config_.pin_href = href;
  this->camera_config_.pin_pclk = pclk;
  this->camera_config_.pin_pwdn = pwdn;
  this->camera_config_.pin_reset = reset;
  this->camera_config_.pin_sccb_sda = -1;
  this->camera_config_.pin_sccb_scl = -1;
  this->camera_config_.ledc_timer = LEDC_TIMER_0;
  this->camera_config_.ledc_channel = LEDC_CHANNEL_0;

  switch (this->image_spec_->format) {
    case camera::PIXEL_FORMAT_GRAYSCALE: {
      this->camera_config_.pixel_format = PIXFORMAT_GRAYSCALE;
    } break;
    case camera::PIXEL_FORMAT_RGB565: {
      this->camera_config_.pixel_format = PIXFORMAT_RGB565;
    } break;
    case camera::PIXEL_FORMAT_BGR888: {
      this->camera_config_.pixel_format = PIXFORMAT_RGB888;
    } break;
  }
  // No direct JPEG currently.
  //    this->camera_config_.pixel_format = PIXFORMAT_JPEG;
  //    this->camera_config_.jpeg_quality = 12; //0-63, for OV series camera sensors, lower number means higher quality
  this->camera_config_.jpeg_quality = 0;  // 0-63, for OV series camera sensors, lower number means higher quality
  this->camera_config_.fb_count =
      2;  // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
  this->camera_config_.fb_location = CAMERA_FB_IN_PSRAM;    // CAMERA_FB_IN_DRAM
  this->camera_config_.grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // CAMERA_GRAB_LATEST
  // this->camera_config_.grab_mode = CAMERA_GRAB_LATEST; // CAMERA_GRAB_LATEST
}

camera::SensorError ESP32CameraSensor::capture_pixels() {
  if (this->frame_buffer_in_use_) {
    esp_camera_fb_return(this->frame_buffer_in_use_);
    this->frame_buffer_in_use_ = nullptr;
  }

  this->frame_buffer_in_use_ = esp_camera_fb_get();

  if (this->frame_buffer_in_use_) {
    this->buffer_.set_frame_buffer(this->frame_buffer_in_use_);
    return camera::SensorError::SENSOR_ERROR_SUCCESS;
  }

  return camera::SensorError::SENSOR_ERROR_RETRY;
}

bool ESP32CameraSensor::camera_sensor_setup() {
  this->camera_config_.sccb_i2c_port = this->i2c_bus_->get_port();
  esp_err_t err = esp_camera_init(&this->camera_config_);
  if (err == ESP_OK)
    return true;

  ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
  return false;
}

void ESP32CameraSensor::camera_sensor_dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 Camera Sensor:\n"
                "  Resolution: %dx%d\n"
                "  xclk_freq_hz %d\n"
                "  %s\n",
                this->image_spec_->width, this->image_spec_->height, this->camera_config_.xclk_freq_hz,
                to_string(this->image_spec_->format));
}

}  // namespace esphome::camera_sensor

#endif
