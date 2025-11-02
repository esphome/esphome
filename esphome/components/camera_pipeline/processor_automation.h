#pragma once

#include "esphome/components/camera/callback_manager_reentry.h"

#include "processor_base.h"

namespace esphome::camera_pipeline {

/// Template base class adding automation support to camera processors.
/// Provides pre- and post-processing automation callbacks.
template<typename T> class ProcessorAutomation : public ProcessorBase {
 public:
  // ---- ProcessorBase ----
  /// Run pre-processing automation callbacks.
  bool pre_process_automation(camera::CameraImageSpec *input_spec, camera::Buffer *input) override {
    this->input_spec_ = input_spec;
    this->input_ = input;
    T &impl = static_cast<T &>(*this);
    return this->pre_process_callbacks_(impl);
  }
  /// Run post-processing automation callbacks.
  bool post_process_automation() override {
    T &impl = static_cast<T &>(*this);
    return this->post_process_callbacks_(impl);
  }
  // -----------------------
  /// Get the input image specification used during pre-processing automation.
  camera::CameraImageSpec *get_input_image_spec() { return this->input_spec_; }
  /// Get the input image buffer used during pre-processing automation.
  camera::Buffer *get_input_image() { return this->input_; }
  /// Add a pre-processing automation callback.
  void add_pre_process_callback(std::function<void(T &processor, camera::Reentry &reentry)> &&callback) {
    this->pre_process_callbacks_.add(std::move(callback));
  }
  /// Add a post-processing automation callback.
  void add_post_process_callback(std::function<void(T &processor, camera::Reentry &reentry)> &&callback) {
    this->post_process_callbacks_.add(std::move(callback));
  }

 protected:
  camera::CallbackManagerReentry<T &> pre_process_callbacks_{};
  camera::CallbackManagerReentry<T &> post_process_callbacks_{};
  camera::CameraImageSpec *input_spec_{};
  camera::Buffer *input_{};
};

}  // namespace esphome::camera_pipeline
