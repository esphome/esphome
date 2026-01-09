#pragma once

#include "../dfrobot_c4002.h"
#include "esphome/components/select/select.h"

#define MODE_1 "Mode_1"
#define MODE_2 "Mode_2"
#define MODE_3 "Mode_3"

namespace esphome {
namespace dfrobot_c4002 {

class C4002Select : public Component, public select::Select, public Parented<C4002Component> {
 public:
  C4002Select() = default;

  void set_options(const std::vector<std::string> &options){};

 protected:
  void control(const std::string &value) override;
  std::string options_[3] = {"Mode_1", "Mode_2", "Mode_3"};
};

}  // namespace dfrobot_c4002
}  // namespace esphome
