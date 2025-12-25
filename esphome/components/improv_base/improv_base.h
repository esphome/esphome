#pragma once

#include <string>
#include "esphome/core/defines.h"

namespace esphome {
namespace improv_base {

class ImprovBase {
 public:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  void set_next_url(const std::string &next_url) { this->next_url_ = next_url; }
#endif

 protected:
#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
  std::string get_formatted_next_url_();
  std::string next_url_;
#endif
};

}  // namespace improv_base
}  // namespace esphome
