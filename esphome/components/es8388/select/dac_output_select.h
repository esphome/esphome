#pragma once

#include "esphome/components/es8388/es8388.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace es8388 {

class DacOutputSelect : public select::Select, public Parented<ES8388> {
 protected:
  void control(size_t index) override;
};

}  // namespace es8388
}  // namespace esphome
