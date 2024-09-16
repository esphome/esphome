#pragma once

#include "../ld2420.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace ld2420 {

class LD2420Select : public Component, public select::Select, public Parented<LD2420Component> {
 public:
  LD2420Select() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2420
}  // namespace esphome
