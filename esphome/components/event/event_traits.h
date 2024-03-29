#pragma once

#include <vector>
#include <string>

namespace esphome {
namespace event {

class EventTraits {
 public:
  void set_types(std::vector<std::string> types);
  std::vector<std::string> get_types() const;

 protected:
  std::vector<std::string> types_;
};

}  // namespace event
}  // namespace esphome
