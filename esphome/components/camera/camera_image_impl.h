#pragma once

#include <utility>

#include "buffer.h"
#include "camera.h"
#include "delete_callback.h"
#include "requester_flags.h"

namespace esphome::camera {

/** Camera image interface implemenation.
 */
class CameraImageImpl : public CameraImage, public DeleteCallback<CameraImageImpl> {
 public:
  void set_buffer(Buffer *buffer) { this->buffer_ = buffer; }
  Buffer *get_buffer() { return this->buffer_; }
  // Specifies the filter used in add_image_callback.
  void set_requesters(const RequesterFlags &requesters) { this->requesters_ = std::move(requesters); }

  // ---- CameraImage interface ----
  uint8_t *get_data_buffer() override { return this->buffer_->get_data(); }
  size_t get_data_length() override { return this->buffer_->get_size(); }
  bool was_requested_by(CameraRequester requester) const override { return this->requesters_.has(requester); }
  // -------------------------------

 protected:
  Buffer *buffer_{};
  RequesterFlags requesters_{};
};

}  // namespace esphome::camera
