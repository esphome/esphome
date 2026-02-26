#pragma once

#include "esphome/core/defines.h"

#include "esphome/core/component.h"

namespace esphome {
namespace watchdog {

class WatchdogManager {
 public:
  WatchdogManager() {}
  WatchdogManager(uint32_t timeout_ms);
  ~WatchdogManager();

 protected:
  uint32_t get_timeout_();
  void set_timeout_(uint32_t timeout_ms);

 private:
  uint32_t saved_timeout_ms_{0};
  uint32_t timeout_ms_{0};
};

class WatchdogManagerComponent : public Component, WatchdogManager {
 public:
  void dump_config() override;
  void set_timeout(uint32_t timeout_ms) { this->set_timeout_(timeout_ms); }
};

}  // namespace watchdog
}  // namespace esphome
