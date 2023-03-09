#pragma once

#include "esphome/components/select/select.h"

namespace esphome {
namespace ld2410 {

class LD2410Select : public select::Select {
 public:
  LD2410Select();

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2410
}  // namespace esphome
