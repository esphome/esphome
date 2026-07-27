#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR

#include "esphome/core/hal.h"
#include "esphome/core/wake.h"

#include <zephyr/kernel.h>
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
#endif

namespace esphome {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
K_SEM_DEFINE(esphome_wake_sem, 0, 1);

// === Wake-requested flag storage ===
// On single-core targets (ESPHOME_THREAD_SINGLE), volatile uint8_t is enough: the
// store/load is a single non-tearing instruction, and k_sem_give()/k_sem_take() already
// provide the release/acquire barrier. native_sim simulates interrupts via host-level
// mechanisms rather than single-core preemption, so it needs a real atomic instead.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<uint8_t> g_wake_requested{0};
#else
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;
#endif

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
