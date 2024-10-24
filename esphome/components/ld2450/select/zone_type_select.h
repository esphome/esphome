#pragma once

#include "esphome/components/select/select.h"
#include "../ld2450.h"

namespace esphome {
namespace ld2450 {

class ZoneTypeSelect : public select::Select, public Parented<LD2450Component> {
 public:
  ZoneTypeSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace ld2450
}  // namespace esphome
