#pragma once

#include "esphome/components/camera/task.h"
#include "esphome/components/camera/processor.h"

namespace esphome::camera_pipeline {

extern const char *const TAG;

/// Base class for all camera pipeline processors.
/// Implements automation handling, optional job execution, and statistics logging.
class ProcessorBase : public camera::Processor {
 public:
  /// Assign the processing task.
  void set_task(camera::Task *task) { this->task_ = task; }
  /// Set processor ID used for identification and logging.
  void set_id(std::string id) { this->id_ = id; }
  /// Enable or disable performance statistics logging.
  void set_statistics(bool statistics) { this->statistics_ = statistics; }
  /// Run processor in a separate job to avoid blocking.
  void set_run_as_job(bool run_as_job) { this->run_as_job_ = run_as_job; }
  // ------ Processor ------
  const char *get_id() const override { return this->id_.c_str(); }
  bool configure() override { return true; }
  camera::ProcessorError process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) override;
  camera::ProcessorError process_compressed_image(camera::ImageFormat input_format, camera::Buffer *input) override;
  camera::ImageFormat get_output_image_format() override { return camera::ImageFormat::IMAGE_FORMAT_RAW; }
  void release_resources() override {}
  // ------------------------
  /// Process RAW pixels (implemented by specific processor).
  virtual camera::ProcessorError process_pixels_base(camera::CameraImageSpec *input_spec, camera::Buffer *input) = 0;
  /// Process compressed image data (implemented by specific processor).
  virtual camera::ProcessorError process_compressed_image_base(camera::ImageFormat input_format,
                                                               camera::Buffer *input) {
    return camera::PROCESSOR_ERROR_CONFIGURATION;
  }
  /// Run pre-processing automation in main task.
  /// @return True if all callbacks completed, false if more calls are required.
  virtual bool pre_process_automation(camera::CameraImageSpec *input_spec, camera::Buffer *input) = 0;
  /// Run post-processing automation in main task.
  /// @return True if all callbacks completed, false if more calls are required.
  virtual bool post_process_automation() = 0;

 protected:
  camera::ProcessorError run_state_machine_(camera::ImageFormat input_format, camera::CameraImageSpec *input_spec,
                                            camera::Buffer *input);

  enum State : uint8_t {
    STATE_NEW_IMAGE = 0,
    STATE_PRE_AUTOMATION,
    STATE_MAIN_TASK_PROCESSING,
    STATE_START_JOB_TASK,
    STATE_JOB_TASK_PROCESSING,
    STATE_HANDLE_RESULT,
    STATE_POST_AUTOMATION
  };

  State state_{STATE_NEW_IMAGE};
  bool run_as_job_{};
  bool statistics_{};
  uint32_t pre_processing_time_{};
  uint32_t processing_time_{};
  uint32_t post_processing_time_{};
  uint32_t pre_entries_{};
  uint32_t entries_{};
  uint32_t post_entries_{};
  camera::ProcessorError error_{};
  camera::Task *task_{};
  std::string id_{};
};

}  // namespace esphome::camera_pipeline
