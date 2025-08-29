#pragma once

#include "camera.h"
#include "camera_image_impl.h"
#include "encoder.h"
#include "processor.h"
#include "sensor.h"

namespace esphome {
namespace camera {

/** Camera interface implemenation.
 */
class CameraImpl : public Camera {
 public:
  enum CameraState : uint8_t {
    CAMERA_STATE_INIT = 0,
    CAMERA_STATE_WAIT_FOR_REQUEST,
    CAMERA_STATE_CAPTURE_BEGIN,
    CAMERA_STATE_CAPTURING,
    CAMERA_STATE_PROCESSING,
    CAMERA_STATE_OVERLAY_BEGIN,
    CAMERA_STATE_OVERLAYING,
    CAMERA_STATE_ENCODE_BEGIN,
    CAMERA_STATE_ENCODING,
    CAMERA_STATE_RATE_LIMITING,
    CAMERA_STATE_PUBLISHING,
    CAMERA_STATE_CLEAR_REQUEST,
  };

  // Sets the update interval in milliseconds for images without requests
  void set_idle_update_interval(uint32_t idle_update_interval) { this->idle_update_interval_ = idle_update_interval; }
  // Sets the number of milliseconds between two consecutive images
  void set_max_update_interval(uint32_t max_update_interval) { this->max_update_interval_ = max_update_interval; }
  // Sets sensor implementation
  void set_sensor(Sensor *sensor) { this->sensor_ = sensor; }
  // Sets encoder implementation
  void set_encoder(Encoder *encoder) { this->encoder_ = encoder; }
  // Performs camera processing tasks such as image capture and JPEG encoding spread
  // over multiple loop cycles to avoid blocking and ensure real-time responsiveness.
  // This method is intended to be called repeatedly from the loop.
  // It performs a portion of the image capture or encoding process each time it runs.
  // Returns true if additional processing is needed and the loop should continue running.
  // Returns false if all camera-related tasks are complete.
  bool camera_loop();
  // ---- Camera interface ----
  CameraImageReader *create_image_reader() override;
  void request_image(CameraRequester requester) override;
  void start_stream(CameraRequester requester) override;
  void stop_stream(CameraRequester requester) override;
  // -------- Component -------
  void setup() override;
  void loop() override;
  void dump_config() override;
  // --------------------------
  // Appends a processor that will run after image capture, or after any previously
  // appended processors, but always before any overlays are applied.
  // This allows chaining multiple processors, such as scalers or colorizers,
  // in a specific order of execution,
  void append_processor(Processor *processor) { this->processors_.push_back(processor); }

 protected:
  Buffer *input_image_{};
  CameraImageSpec *input_image_spec_{};
  std::shared_ptr<CameraImageAdapter> jpeg_;
  CameraIncrementalContext camera_incremental_context_;
  CameraState state_{CAMERA_STATE_INIT};
  std::vector<Processor *> processors_;
  Sensor *sensor_{};
  Encoder *encoder_{};
  uint8_t image_requesters_{};
  uint8_t stream_requesters_{};
  uint32_t last_idle_request_{};
  uint32_t idle_update_interval_{};
  uint32_t last_update_{};
  uint32_t max_update_interval_{};
  uint32_t skip_frame_counter_{};
  uint32_t retry_frame_counter_{};
};

}  // namespace camera
}  // namespace esphome
