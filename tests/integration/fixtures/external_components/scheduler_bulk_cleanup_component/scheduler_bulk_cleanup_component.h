#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"

namespace esphome::scheduler_bulk_cleanup_component {

class SchedulerBulkCleanupComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void trigger_bulk_cleanup();
};

}  // namespace esphome::scheduler_bulk_cleanup_component
