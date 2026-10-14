#pragma once

#include "esphome/components/select/select.h"
#include "../hon_climate.h"

namespace esphome::haier {

class VerticalAirflowSelect : public select::Select, public Parented<HonClimate> {
 protected:
  void control(size_t index) override { this->parent_->set_vertical_airflow(VERTICAL_SWING_MODES_ORDER[index]); }
};

}  // namespace esphome::haier
