#pragma once

#include "camera.h"
#include "sensor.h"
#include "delete_callback.h"

namespace esphome::camera {

/** Camera image interface implemenation.
 */
class CameraImageImpl : public CameraImage, public DeleteCallback<CameraImageImpl> {
 public:
  void set_camera_sensor(Sensor *sensor) { this->sensor_ = sensor; }
  void set_buffer(Buffer *buffer) { this->buffer_ = buffer; }
  Buffer *get_buffer() { return this->buffer_; }
  // Specifies the filter used in add_image_callback.
  void set_requesters(uint8_t requesters) { this->requesters_ = requesters; }

  // ---- CameraImage interface ----
  uint8_t *get_data_buffer() override { return this->buffer_->get_data(); }
  size_t get_data_length() override { return this->buffer_->get_size(); }
  bool was_requested_by(CameraRequester requester) const override {
    return (this->requesters_ & (1 << requester)) != 0;
  }
  // -------------------------------

 protected:
  Buffer *buffer_{};
  Sensor *sensor_{};
  uint8_t requesters_{};
};

}  // namespace esphome::camera
