#pragma once

#include <string>

namespace esphome {
namespace improv_base {

class ImprovBase {
 public:
  void set_next_url(const std::string &next_url) { this->next_url_ = next_url; }

 protected:
  std::string get_formatted_next_url_();
  std::string next_url_;
};

}  // namespace improv_base
}  // namespace esphome
