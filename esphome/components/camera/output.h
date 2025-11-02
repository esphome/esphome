#pragma once

#include "camera_image_impl.h"

#include <memory>

namespace esphome::camera {

/// Interface for the final stage in the camera pipeline.
/// Produces an image for CameraRequesters.
class Output {
 public:
  /// Get the latest processed image.
  virtual std::shared_ptr<CameraImageImpl> get_image() = 0;
  virtual ~Output() = default;
};

}  // namespace esphome::camera
