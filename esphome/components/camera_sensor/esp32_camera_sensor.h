#pragma once

#ifdef USE_ESP32_CAMERA_SENSOR

#include "esphome/components/camera/buffer_pool.h"
#include "esphome/components/camera/sensor.h"
#include "esphome/components/i2c/i2c_bus.h"

#include <esp_camera.h>
#include <sensor.h>

namespace esphome::camera_sensor {

class ESP32CameraSensorBuffer : public camera::Buffer {
 public:
  void set_frame_buffer(camera_fb_t *frame_buffer) { this->frame_buffer_ = frame_buffer; }
  camera_fb_t *get_frame_buffer() { return this->frame_buffer_; }
  // -------- Buffer --------
  uint8_t *get_data() const override { return this->frame_buffer_->buf; }
  size_t get_size() const override { return this->frame_buffer_->len; }

 protected:
  camera_fb_t *frame_buffer_{};
};

class ESP32CameraSensor : public camera::Sensor {
 public:
  ESP32CameraSensor(uint16_t width, uint16_t height);
  void set_pins(int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7, int xclk, int vsync, int href, int pclk,
                int pwdn, int reset);
  void set_i2c_bus(i2c::InternalI2CBus *i2c_bus) { this->i2c_bus_ = i2c_bus; }
  void set_frequency(uint32_t frequency) { this->camera_config_.xclk_freq_hz = frequency; }
  void set_framesize(framesize_t framesize) { this->camera_config_.frame_size = framesize; }
  void set_buffers(uint16_t buffers);
  void set_pixel_format(camera::PixelFormat pixel_format);
  void set_jpeg_quality(unsigned int jpeg_quality);
  // -------- Sensor --------
  bool configure() override;
  camera::Buffer *acquire_frame_buffer() override;
  void return_frame_buffer(camera::Buffer *buffer) override;
  camera::Resolution get_resolution() override { return this->resolution_; }
  camera::ImageFormat get_image_format() override;
  std::optional<camera::PixelFormat> get_pixel_format() override { return this->pixel_format_; }
  void log_config() override;
  // -------------------------

 protected:
  camera_config_t camera_config_{};
  i2c::InternalI2CBus *i2c_bus_{};
  camera::Resolution resolution_{};
  std::optional<camera::PixelFormat> pixel_format_{};
  camera::BufferPool<ESP32CameraSensorBuffer> pool_;
};

}  // namespace esphome::camera_sensor

#endif
