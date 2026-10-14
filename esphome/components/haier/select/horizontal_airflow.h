#pragma once

#include "esphome/components/select/select.h"
#include "../hon_climate.h"

namespace esphome::haier {

class HorizontalAirflowSelect : public select::Select, public Parented<HonClimate> {
 protected:
  void control(size_t index) override {
    this->parent_->set_horizontal_airflow(airflow_order_at(HORIZONTAL_SWING_MODES_ORDER, index));
  }
};

}  // namespace esphome::haier
