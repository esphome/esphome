#pragma once

#include <cstddef>
#include "esphome/core/defines.h"

#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
#include <improv.h>
#endif

namespace esphome::improv_base {

class ImprovBase {
 public:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  void set_next_url(const char *next_url) { this->next_url_ = next_url; }
#endif

 protected:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  /// Format next_url_ into buffer, replacing placeholders. Returns length written.
  size_t get_formatted_next_url_(char *buffer, size_t buffer_size);
  /// Append the formatted next_url to the RPC response, warning if it does not fit.
  void add_next_url_(improv::RpcResponseBuilder &builder, size_t max_len);
  const char *next_url_{nullptr};
#endif
};

}  // namespace esphome::improv_base
