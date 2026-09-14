#include "mock_camera.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mock_camera {

static const char *const TAG = "mock_camera";

void MockCamera::loop() {
  uint8_t requesters = this->single_requesters_ | this->stream_requesters_;
  if (requesters == 0)
    return;
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_frame_ms_ < FRAME_INTERVAL_MS)
    return;
  this->last_frame_ms_ = now;
  this->single_requesters_ = 0;

  auto image = std::make_shared<MockCameraImage>(this->image_size_, this->frame_counter_, requesters);
  ESP_LOGV(TAG, "Producing frame %u (%u bytes, requesters 0x%02X)", this->frame_counter_, this->image_size_,
           requesters);
  this->frame_counter_++;
  for (auto *listener : this->listeners_) {
    listener->on_camera_image(image);
  }
}

void MockCamera::dump_config() { ESP_LOGCONFIG(TAG, "Mock Camera (%u byte frames)", this->image_size_); }

}  // namespace esphome::mock_camera
