#pragma once

#include "esphome/core/component.h"
#include "esphome/components/power_management/power_management.h"

#include <memory>

#ifdef USE_ESP_IDF
#include "esp_pm.h"

namespace esphome {
namespace esp32_pm {

class ESPPMLock : public power_management::PMLock {
 public:
  ESPPMLock(const std::string &name, power_management::PmLockType lock);
  ~ESPPMLock();

 private:
  std::string name_;
  power_management::PmLockType lock_;

#ifdef USE_ESP_IDF
  esp_pm_lock_handle_t pm_lock_;
#endif
};

class ESP32PowerManagement : public power_management::PowerManagement {
 public:
  void setup() override;
  void set_freq(uint16_t min_freq_mhz, uint16_t max_freq_mhz);
  float get_setup_priority() const override { return setup_priority::BUS; }
  void set_tickless(bool tickless);
  void loop() override;
  void dump_config() override;

  std::unique_ptr<power_management::PMLock> get_lock(std::string name, power_management::PmLockType lock);

 private:
  uint16_t min_freq_ = 40;
  uint16_t max_freq_ = 240;
  bool tickless_ = false;
  bool setup_done_ = false;
#ifdef USE_ESP_IDF
  esp_pm_lock_handle_t startup_lock_;
#endif
};

}  // namespace esp32_pm
}  // namespace esphome
#endif
