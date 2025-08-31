#include "software_sensor.h"

namespace esphome {
namespace camera_sensor {

static const char *const TAG = "camera_sensor";

SoftwareSensor::SoftwareSensor(camera::CameraImageSpec *spec, camera::Buffer *buffer) {
  this->image_spec_ = spec;
  this->buffer_ = buffer;
}

void SoftwareSensor::camera_sensor_dump_config() {
  ESP_LOGCONFIG(TAG,
                "Software Sensor:\n"
                "  Resolution: %dx%d\n"
                "  %s\n",
                this->image_spec_->width, this->image_spec_->height, to_string(this->image_spec_->format));
}

}  // namespace camera_sensor
}  // namespace esphome
