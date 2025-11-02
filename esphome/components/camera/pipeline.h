#pragma once

#include "processor.h"
#include "output.h"
#include "requester_flags.h"

#include <unordered_map>
#include <unordered_set>

namespace esphome::camera {

/// Error codes returned from the camera pipeline.
enum PipelineError : uint8_t {
  PIPELINE_ERROR_SUCCESS = 0,   ///< Processing succeeded, output ready.
  PIPELINE_ERROR_SKIP_FRAME,    ///< Skip current frame, try next.
  PIPELINE_ERROR_REENTER,       ///< Reenter pipeline, processor not done yet.
  PIPELINE_ERROR_CONFIGURATION  ///< Configuration error, pipeline stopped.
};

/// Pipeline manages a set of camera processors and controls frame flow.
/// Each processor can have multiple children but only one parent.
/// Supports reentry for processors that need multiple passes per frame,
/// default outputs, requester-specific outputs, and cycle detection.
class Pipeline {
 public:
  /// Set the input (root) processor.
  void set_input(Processor *input) { this->input_ = input; }
  /// Set the default output processor used when no requester-specific one exists.
  void set_default_output(Output *default_output) { this->default_output_ = default_output; }
  /// Add an output processor for a specific requester.
  void add_output(Output *output, CameraRequester requester) { this->outputs_[requester] = output; }
  /// Link a processor to its child.
  void add_link(Processor *parent, Processor *child);
  /// Add a processor, mainly to detect unlinked processors.
  void add_processor(Processor *processor) { this->all_processors_.insert(processor); }
  /// Get all output processors matching the given requester flags.
  std::unordered_set<Output *> filter_outputs(RequesterFlags flags);
  /// Filter requester flags based on which output they map to.
  RequesterFlags filter_requesters(Output *output, RequesterFlags flags);
  /// Validate pipeline configuration and detect cycles.
  bool configure();
  /// Process one frame through the pipeline.
  PipelineError process();
  /// Log current pipeline configuration.
  void log_config();

 protected:
  Processor *find_next_(Processor *current);
  Processor *find_parent_(Processor *current);
  Processor *find_cycle_();
  std::unordered_set<Processor *> find_unlinked_();
  std::unordered_map<CameraRequester, Output *> outputs_;
  std::unordered_map<Processor *, std::vector<Processor *> > links_;
  std::unordered_map<Processor *, Processor *> parents_;
  std::unordered_set<Processor *> traversed_;
  std::unordered_set<Processor *> all_processors_;
  Output *default_output_{};
  Processor *input_{};
  Processor *current_{};
  Processor *parent_{};
  ImageFormat input_image_format_;
  CameraImageSpec input_image_spec_;
  Buffer *input_image_{};
  bool reenter_{};
};

}  // namespace esphome::camera
