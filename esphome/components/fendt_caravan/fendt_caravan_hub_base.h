#pragma once

#ifdef USE_ESP32
#include "esphome/core/component.h"

namespace esphome::fendt_caravan {

class FendtCaravanHubBase : public PollingComponent {
 public:
  virtual bool decode(const char *name, const char *value) { return false; }
  void dump_config() override = 0;
};

}  // namespace esphome::fendt_caravan
#endif
