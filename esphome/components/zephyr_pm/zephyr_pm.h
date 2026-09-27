#pragma once

#ifdef USE_ZEPHYR

#include "esphome/core/component.h"
#include "esphome/components/power_management/power_management.h"

namespace esphome::zephyr_pm {

class ZephyrPowerManagement : public power_management::PowerManagementComponent {
 public:
  float get_setup_priority() const override { return setup_priority::POWER; }
  void dump_config() override;
};

}  // namespace esphome::zephyr_pm

#endif
