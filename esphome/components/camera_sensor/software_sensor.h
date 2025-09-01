#pragma once

#include "esphome/components/camera/sensor.h"

namespace esphome {
namespace camera_sensor {

class SoftwareSensor : public camera::Sensor {
 public:
  SoftwareSensor(camera::CameraImageSpec *spec, camera::Buffer *buffer);
  // -------- Sensor --------
  camera::SensorError capture_pixels() override { return camera::SensorError::SENSOR_ERROR_SUCCESS; }
  camera::Buffer *get_image_buffer() override { return this->buffer_; }
  camera::CameraImageSpec *get_image_spec() override { return this->image_spec_; }
  bool camera_sensor_setup() override { return true; }
  void camera_sensor_dump_config() override;
  // -------------------------

 protected:
  camera::Buffer *buffer_{};
  camera::CameraImageSpec *image_spec_;
};

}  // namespace camera_sensor
}  // namespace esphome
