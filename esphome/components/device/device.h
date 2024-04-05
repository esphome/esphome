#pragma once

namespace esphome {
namespace device {

class Device {
 public:
  void set_name(std::string name) { name_ = name; }
  std::string get_name(void) { return name_; }

 protected:
  std::string name_ = "";
};

}  // namespace device
}  // namespace esphome
