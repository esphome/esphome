#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

#include <zephyr/kernel.h>

namespace esphome {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
K_SEM_DEFINE(esphome_wake_sem, 0, 1);

// === Wake-requested flag storage ===
// Zephyr/NRF52 is ESPHOME_THREAD_SINGLE.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;

void wake_loop_threadsafe() {
  wake_request_set();
  k_sem_give(&esphome_wake_sem);
}

namespace internal {
void wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  k_sem_take(&esphome_wake_sem, ms == UINT32_MAX ? K_FOREVER : K_MSEC(ms));
}
}  // namespace internal

}  // namespace esphome

#endif  // USE_ZEPHYR
