#pragma once

#include "esphome/components/select/select.h"
#include "../mr24hpc1.h"

namespace esphome {
namespace mr24hpc1 {

class SceneModeSelect : public select::Select, public Parented<mr24hpc1Component> {
  public:
    SceneModeSelect() = default;

  protected:
    void control(const std::string &value) override;
};

}  // namespace mr24hpc1
}  // namespace esphome
