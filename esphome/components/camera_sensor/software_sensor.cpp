#include "software_sensor.h"

#include <cstring>

namespace esphome::camera_sensor {

static const char *const TAG = "camera_sensor";

SoftwareSensor::SoftwareSensor(uint16_t width, uint16_t height, camera::PixelFormat pixel_format)
    : image_spec_(width, height, pixel_format) {}

bool SoftwareSensor::configure() {
  return this->pool_.init(buffers_, [this]() -> camera::BufferImpl * {
    camera::BufferImpl *buffer = new camera::BufferImpl(&this->image_spec_);
    if (!buffer || !buffer->get_data()) {
      ESP_LOGE(TAG, "Failed to allocate buffer of %zu bytes.", this->image_spec_.bytes_per_image());
      delete buffer;
      return nullptr;
    }

    return buffer;
  });
}

camera::Buffer *SoftwareSensor::acquire_frame_buffer() {
  camera::BufferImpl *buffer = this->pool_.acquire();
  if (!buffer)
    return nullptr;

  if (clear_)
    memset(buffer->get_data(), 0, buffer->get_size());

  return buffer;
}

void SoftwareSensor::return_frame_buffer(camera::Buffer *buffer) {
  this->pool_.release(static_cast<camera::BufferImpl *>(buffer));
}

void SoftwareSensor::log_config() {
  ESP_LOGCONFIG(TAG,
                "Software Camera Sensor:\n"
                "  Resolution: %ux%u\n"
                "  Buffers: %zu\n"
                "  %s\n",
                this->image_spec_.width, this->image_spec_.height, this->buffers_, to_string(this->image_spec_.format));
}

}  // namespace esphome::camera_sensor
