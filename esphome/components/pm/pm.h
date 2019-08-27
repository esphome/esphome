#pragma once

#include "esphome/core/component.h"

#include <memory>

#ifdef USE_ESP_IDF
#include "esp_pm.h"
#endif

namespace esphome {
namespace pm {

class PMLock {
 public:
  PMLock(std::shared_ptr<esp_pm_lock_handle_t> pm_lock);
  ~PMLock();

 private:
#ifdef USE_ESP_IDF
  std::shared_ptr<esp_pm_lock_handle_t> pm_lock_;
#endif
};

class PM : public Component {
 public:
  void setup() override;
  void disable();
  void set_freq(uint16_t min_freq_mhz, uint16_t max_freq_mhz);

  void set_tickless(bool tickless);

  std::unique_ptr<pm::PMLock> get_lock();

 private:
  uint16_t min_freq = 40;
  uint16_t max_freq = 240;
  bool tickless = false;
#ifdef USE_ESP_IDF
  std::shared_ptr<esp_pm_lock_handle_t> pm_lock_;
#endif
};

extern PM *global_pm;

}  // namespace pm
}  // namespace esphome
