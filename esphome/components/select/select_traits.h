#pragma once

#include <vector>
#include <string>

namespace esphome {
namespace select {

class SelectTraits {
 public:
  void set_options(std::vector<std::string> options);
  std::vector<std::string> get_options() const;

 protected:
  std::vector<std::string> options_;
};

}  // namespace select
}  // namespace esphome
