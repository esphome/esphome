#pragma once

#include "esphome/components/camera/buffer_impl.h"
#include "esphome/components/camera/buffer_pool.h"
#include "esphome/components/camera/sensor.h"

namespace esphome::camera_sensor {

class SoftwareSensor : public camera::Sensor {
 public:
  SoftwareSensor(uint16_t width, uint16_t height, camera::PixelFormat pixel_format);
  void set_buffers(uint16_t buffers) { this->buffers_ = buffers; }
  void set_clear(bool clear) { this->clear_ = clear; }
  // -------- Sensor --------
  bool configure() override;
  camera::Buffer *acquire_frame_buffer() override;
  void return_frame_buffer(camera::Buffer *buffer) override;
  camera::Resolution get_resolution() override { return this->image_spec_; }
  camera::ImageFormat get_image_format() override { return camera::IMAGE_FORMAT_RAW; }
  std::optional<camera::PixelFormat> get_pixel_format() override { return this->image_spec_.format; }
  void log_config() override;
  // -------------------------

 protected:
  bool clear_{};
  uint16_t buffers_{};
  camera::CameraImageSpec image_spec_;
  camera::BufferPool<camera::BufferImpl> pool_;
};

}  // namespace esphome::camera_sensor
