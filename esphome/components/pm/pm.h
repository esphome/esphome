#pragma once

#include "esphome/core/component.h"

#include <memory>

#ifdef USE_ESP_IDF
#include "esp_pm.h"

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
  void set_freq(uint16_t min_freq_mhz, uint16_t max_freq_mhz);
  float get_setup_priority() const { return setup_priority::BUS; }
  void set_tickless(bool tickless);
  void loop();
  void dump_config() override;

  std::unique_ptr<pm::PMLock> get_lock();

 private:
  uint16_t min_freq_ = 40;
  uint16_t max_freq_ = 240;
  bool tickless_ = false;
  bool setup_done_ = false;
#ifdef USE_ESP_IDF
  std::shared_ptr<esp_pm_lock_handle_t> pm_lock_;
  esp_pm_lock_handle_t startup_lock_;
#endif
};

extern PM *global_pm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace pm
}  // namespace esphome
#endif
