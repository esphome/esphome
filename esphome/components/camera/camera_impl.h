#pragma once

#include "esphome/core/helpers.h"

#include "camera.h"
#include "camera_image_impl.h"
#include "camera_image_reader_impl.h"
#include "task.h"
#include "processor.h"
#include "pipeline.h"

#include <queue>

namespace esphome::camera {

/// Camera pipeline implemenation.
class CameraImpl : public Camera {
 public:
  /// Sets the update interval in milliseconds for images without requests
  void set_idle_update_interval(uint32_t idle_update_interval) { this->idle_update_interval_ = idle_update_interval; }
  /// Sets the number of milliseconds between two consecutive images
  void set_max_update_interval(uint32_t max_update_interval) { this->max_update_interval_ = max_update_interval; }
  /// Prints framerate and per-frame processing timings
  void set_statistics(bool statistics) { this->statistics_ = statistics; }
  /// Set the task that is used by the pipeline.
  void set_task(Task *task) { this->task_ = task; }
  /// Set the pipeline instance.
  void set_pipeline(Pipeline *pipeline) { this->pipeline_ = pipeline; }
  // ---- Camera interface ----
  CameraImageReader *create_image_reader() override { return new CameraImageReaderImpl; }
  void request_image(CameraRequester requester) override;
  void start_stream(CameraRequester requester) override;
  void stop_stream(CameraRequester requester) override;
  // -------- Component -------
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  // --------------------------

 protected:
  enum CameraState : uint8_t {
    CAMERA_STATE_WAIT_FOR_REQUEST = 0,
    CAMERA_STATE_PROCESSING,
    CAMERA_STATE_RATE_LIMITING,
    CAMERA_STATE_PUBLISHING,
    CAMERA_STATE_CLEAR_REQUEST,
  };

  CameraState state_{CAMERA_STATE_WAIT_FOR_REQUEST};
  RequesterFlags next_requesters_{};
  RequesterFlags current_requesters_{};
  RequesterFlags stream_requesters_{};
  bool is_publishing_{};
  bool statistics_{};
  bool stream_started_{};
  bool stream_stoped_{};
  uint32_t last_idle_request_{};
  uint32_t idle_update_interval_{};
  uint32_t last_update_{};
  uint32_t next_update_{};
  uint32_t max_update_interval_{};
  uint32_t timing_fps_{};
  uint32_t processing_time_{};
  uint32_t limiter_time_{};
  uint32_t entries_{};
  Task *task_{};
  Pipeline *pipeline_{};
  std::unordered_set<Output *> outputs_;
  std::unordered_set<Output *>::iterator current_output_;
  std::queue<std::shared_ptr<CameraImageImpl> > send_queue_{};
  Mutex lock_{};
};

}  // namespace esphome::camera
