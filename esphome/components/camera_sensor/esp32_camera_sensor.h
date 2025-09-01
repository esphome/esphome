#pragma once

#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/camera/sensor.h"

#include <esp_camera.h>
#include <sensor.h>

namespace esphome {
namespace camera_sensor {

class ESP32CameraSensorBuffer : public camera::Buffer {
 public:
  void set_frame_buffer(camera_fb_t *frame_buffer) { this->frame_buffer_ = frame_buffer; }
  // -------- Buffer --------
  uint8_t *get_data_buffer() override { return this->frame_buffer_->buf; }
  size_t get_data_length() override { return this->frame_buffer_->len; }

 protected:
  camera_fb_t *frame_buffer_{};
};

class ESP32CameraSensor : public camera::Sensor {
 public:
  ESP32CameraSensor(camera::CameraImageSpec *spec);
  void set_pins(int d0, int d1, int d2, int d3, int d4, int d5, int d6, int d7, int xclk, int vsync, int href, int pclk,
                int pwdn, int reset);
  void set_i2c_bus(i2c::InternalI2CBus *i2c_bus) { this->i2c_bus_ = i2c_bus; }
  void set_frequency(uint32_t frequency) { this->camera_config_.xclk_freq_hz = frequency; }
  void set_framesize(framesize_t framesize) { this->camera_config_.frame_size = framesize; }
  // -------- Sensor --------
  camera::SensorError capture_pixels() override;
  camera::Buffer *get_image_buffer() override { return &this->buffer_; }
  camera::CameraImageSpec *get_image_spec() override { return this->image_spec_; }
  bool camera_sensor_setup() override;
  void camera_sensor_dump_config() override;
  // -------------------------

 protected:
  camera_config_t camera_config_{};
  i2c::InternalI2CBus *i2c_bus_{};
  ESP32CameraSensorBuffer buffer_{};
  camera::CameraImageSpec *image_spec_;
  camera_fb_t *frame_buffer_in_use_{};
};

}  // namespace camera_sensor
}  // namespace esphome
