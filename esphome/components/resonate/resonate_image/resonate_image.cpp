#include "resonate_image.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_IMAGE)

namespace esphome {
namespace resonate {

ResonateImage(ImageFormat format, image::ImageType type, image::Transparency transparency,
              image::Image *placeholder = nullptr, bool is_big_endian = false, int fixed_width = 0,
              int fixed_height = 0)
    : runtime_image::RuntimeImage(format, type, transparency, placeholder, is_big_endian, fixed_width) {}

}  // namespace resonate
}  // namespace esphome

#endif
