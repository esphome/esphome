#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#if !defined(USE_RP2040) && !defined(USE_HOST)

#ifdef USE_ESP32
#include <esp_system.h>
#endif

namespace esphome {
namespace factory_reset {
class FactoryResetComponent : public Component {
 public:
  FactoryResetComponent(uint8_t required_count, uint32_t max_interval)
      : required_count_(required_count), max_interval_(max_interval) {}

  void dump_config() override;
  void setup() override;
  void add_increment_callback(std::function<void(uint8_t, uint8_t)> &&callback) {
    this->increment_callback_.add(std::move(callback));
  }

 protected:
  ~FactoryResetComponent() = default;
  void save_(uint8_t count);
  ESPPreferenceObject flash_{};  // saves the number of fast power cycles
  uint8_t required_count_;       // The number of boot attempts before fast boot is enabled
  uint32_t max_interval_;        // max interval between power cycles
  CallbackManager<void(uint8_t, uint8_t)> increment_callback_{};
};

class FastBootTrigger : public Trigger<uint8_t, uint8_t> {
 public:
  explicit FastBootTrigger(FactoryResetComponent *parent) {
    parent->add_increment_callback([this](uint8_t current, uint8_t target) { this->trigger(current, target); });
  }
};
}  // namespace factory_reset
}  // namespace esphome

#endif  // !defined(USE_RP2040) && !defined(USE_HOST)
