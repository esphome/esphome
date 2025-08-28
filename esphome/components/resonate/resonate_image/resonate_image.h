#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_IMAGE)

#include "esphome/components/runtime_image/runtime_image.h"

#include "esphome/core/component.h"

namespace esphome {
namespace resonate {

class ResonateImage : public Component, public runtime_image::RuntimeImage {
 public:
  ResonateImage(ImageFormat format, image::ImageType type, image::Transparency transparency,
                image::Image *placeholder = nullptr, bool is_big_endian = false, int fixed_width = 0,
                int fixed_height = 0);
};

}  // namespace resonate
}  // namespace esphome
#endif
