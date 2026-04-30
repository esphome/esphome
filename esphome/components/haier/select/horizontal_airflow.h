#pragma once

#include "esphome/components/select/select.h"
#include "../hon_climate.h"

namespace esphome {
namespace haier {

class HorizontalAirflowSelect : public select::Select, public Parented<HonClimate> {
 public:
  HorizontalAirflowSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace haier
}  // namespace esphome
