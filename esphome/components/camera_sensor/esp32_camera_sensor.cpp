#ifdef USE_ESP32_CAMERA_SENSOR

#include "esp32_camera_sensor.h"

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

ESP32CameraSensor::ESP32CameraSensor(uint16_t width, uint16_t height) : resolution_{width, height} {}

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
}

void ESP32CameraSensor::set_buffers(uint16_t buffers) { this->camera_config_.fb_count = buffers; }

void ESP32CameraSensor::set_pixel_format(camera::PixelFormat pixel_format) {
  this->pixel_format_ = pixel_format;
  switch (pixel_format) {
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

  this->camera_config_.jpeg_quality = 0;
}

void ESP32CameraSensor::set_jpeg_quality(unsigned int jpeg_quality) {
  this->camera_config_.jpeg_quality = jpeg_quality;
  this->camera_config_.pixel_format = PIXFORMAT_JPEG;
}

bool ESP32CameraSensor::configure() {
  this->pool_.init(this->camera_config_.fb_count, [] { return new ESP32CameraSensorBuffer(); });
  this->camera_config_.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  this->camera_config_.fb_location = CAMERA_FB_IN_PSRAM;
  this->camera_config_.sccb_i2c_port = this->i2c_bus_->get_port();
  esp_err_t err = esp_camera_init(&this->camera_config_);
  if (err == ESP_OK)
    return true;

  ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
  return false;
}

camera::Buffer *ESP32CameraSensor::acquire_frame_buffer() {
  ESP32CameraSensorBuffer *buffer = this->pool_.acquire();
  if (!buffer)
    return nullptr;

  camera_fb_t *frame_buffer = esp_camera_fb_get();
  if (!frame_buffer) {
    this->pool_.release(buffer);
    return nullptr;
  }

  buffer->set_frame_buffer(frame_buffer);
  return buffer;
}

void ESP32CameraSensor::return_frame_buffer(camera::Buffer *buffer) {
  ESP32CameraSensorBuffer *b = reinterpret_cast<ESP32CameraSensorBuffer *>(buffer);
  esp_camera_fb_return(b->get_frame_buffer());
  this->pool_.release(b);
}

camera::ImageFormat ESP32CameraSensor::get_image_format() {
  return this->camera_config_.pixel_format == PIXFORMAT_JPEG ? camera::IMAGE_FORMAT_JPEG : camera::IMAGE_FORMAT_RAW;
}

void ESP32CameraSensor::log_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 Camera Sensor:\n"
                "  Resolution: %ux%u\n"
                "  Buffers: %zu\n"
                "  xclk_freq_hz %d\n"
                "  %s\n",
                this->resolution_.width, this->resolution_.height, this->camera_config_.fb_count,
                this->camera_config_.xclk_freq_hz, to_string(get_image_format()));
  if (this->pixel_format_.has_value())
    ESP_LOGCONFIG(TAG, "  %s\n", to_string(this->pixel_format_.value()));
}

}  // namespace esphome::camera_sensor

#endif
