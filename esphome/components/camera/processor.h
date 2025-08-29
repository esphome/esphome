#pragma once

#include "camera.h"

namespace esphome {
namespace camera {

/** Abstract base class to create a processor that modifies camera images after capture.
 *  A Processor can either modify the input image in-place or produce a separate output image.
 *  This allows chaining multiple processors (e.g., scaler, colorizer) flexibly.
 *
 *  For in-place processing:
 *  - 'process_pixels()' should operate directly on the input buffer.
 *  - 'get_output_image_spec()' should return the same spec as the input.
 *  - 'get_output_image()' should return the same image.
 *
 *  For out-of-place processing (e.g, scaling):
 *  - A new CameraImage instance should be allocated during setup.
 *  - 'get_output_image_spec()' and 'get_output_image()' should return this new instance.
 *
 *  This base class enables chaining processors like: Camera->Scaler->Colorizer->Encoder.
 */
class Processor {
 public:
  virtual size_t process_pixels(camera::CameraImageSpec *input_spec, camera::Buffer *input) = 0;
  virtual camera::CameraImageSpec *get_output_image_spec() = 0;
  virtual camera::Buffer *get_output_image() = 0;
  virtual ~Processor() = default;
};

}  // namespace camera
}  // namespace esphome
