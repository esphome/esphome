#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"

#include <soc/soc_caps.h>
#if SOC_LP_CORE_SUPPORTED
#include <ulp_lp_core.h>
#endif

namespace esphome::esp32_ulp {

class ESP32ULP : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_wakeup_period(uint32_t period_us) { this->wakeup_period_us_ = period_us; }
  void set_run_on_boot(bool run) { this->run_on_boot_ = run; }
  void set_wakeup_from_sleep(bool enable) { this->wakeup_from_sleep_ = enable; }
#if SOC_LP_CORE_SUPPORTED
  void set_wakeup_sources(uint32_t sources) { this->wakeup_sources_ = sources; }
#endif

  /// Load the embedded program into the coprocessor memory and start it.
  bool load_and_run();
  /// Stop the coprocessor timer so the program is not started again.
  void timer_stop();
  /// Resume the coprocessor timer.
  void timer_resume();

 protected:
  bool run_();

  uint32_t wakeup_period_us_{0};
#if SOC_LP_CORE_SUPPORTED
  uint32_t wakeup_sources_{0};
#endif
  bool run_on_boot_{true};
  bool wakeup_from_sleep_{true};
  bool loaded_{false};
};

}  // namespace esphome::esp32_ulp

#endif  // USE_ESP32
