#pragma once

#ifdef USE_ESP32
#include "esphome/core/string_ref.h"
#include "esphome/core/component.h"

namespace esphome::fendt_caravan {

class FendtCaravanHubBase : public PollingComponent {
 public:
  virtual bool decode(const char *name, const char *value) { return false; }
  void setup() override {};
  void loop() override {};
  void dump_config() override = 0;
  void update() override {};
};

}  // namespace esphome::fendt_caravan
#endif
