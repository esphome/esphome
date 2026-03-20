#pragma once

#include <cstddef>
#include "esphome/core/defines.h"

namespace esphome {
namespace improv_base {

class ImprovBase {
 public:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  void set_next_url(const char *next_url) { this->next_url_ = next_url; }
#endif

 protected:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  /// Format next_url_ into buffer, replacing placeholders. Returns length written.
  size_t get_formatted_next_url_(char *buffer, size_t buffer_size);
  const char *next_url_{nullptr};
#endif
};

}  // namespace improv_base
}  // namespace esphome
