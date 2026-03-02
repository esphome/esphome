#include "lt_component.h"

#ifdef USE_LIBRETINY

extern "C" {
extern void sys_log_uart_on(void);
}

#include "esphome/core/log.h"

namespace esphome::libretiny {

static const char *const TAG = "lt.component";

void LTComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LibreTiny:\n"
                "  Version: %s\n"
                "  Loglevel: %u",
                LT_BANNER_STR + 10, LT_LOGLEVEL);
#if defined(__OPTIMIZE_SIZE__) && __OPTIMIZE_LEVEL__ > 0 && __OPTIMIZE_LEVEL__ <= 3
  ESP_LOGCONFIG(TAG, "  Optimization: -Os, SDK: -O" STRINGIFY_MACRO(__OPTIMIZE_LEVEL__));
#endif

#ifdef USE_TEXT_SENSOR
  if (this->version_ != nullptr) {
    this->version_->publish_state(LT_BANNER_STR + 10);
  }
#endif  // USE_TEXT_SENSOR
}

float LTComponent::get_setup_priority() const { return setup_priority::BUS + 500.0f; }  // must be before Logger & UART

void LTComponent::on_powerdown() { this->uart_manager_.deinit_all(); }

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
