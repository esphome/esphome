#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

#include "../duco.h"

namespace esphome::duco {

class DucoSelect : public DucoDevice, public PollingComponent, public select::Select {
 public:
  void update() override;
  bool receive_response(const DucoMessage &message) override;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::duco
