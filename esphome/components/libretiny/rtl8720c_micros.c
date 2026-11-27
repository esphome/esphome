// RTL8720C: LibreTiny's micros() adds SysTick to the tick count and goes back up to 1 ms after a tick missed while
// interrupts are masked, so micros() reads the free-running system timer instead and delayMicroseconds() counts
// SysTick cycles, which keep counting while masked. millis() stays on the tick and falls behind while interrupts are
// masked. Linked with -Wl,--wrap (TIME_WRAPS in __init__.py).

#ifdef USE_LIBRETINY_VARIANT_RTL8720C

#include <stdint.h>
#include "hal_timer.h"

#define SYST_RVR (*(volatile uint32_t *) 0xE000E014)
#define SYST_CVR (*(volatile uint32_t *) 0xE000E018)

// Interrupt handlers call these, so they live in RAM like IRAM_ATTR (hal.h)
#define RTL8720C_RAM_TEXT __attribute__((noinline, section(".sram.text")))

RTL8720C_RAM_TEXT unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  // 32-bit microsecond count that wraps after 71.6 minutes, as micros() does elsewhere
  return hal_read_curtime_us();
}

// Not the system timer: each read takes 2 to 4 us through the ROM, which would stretch short delays
RTL8720C_RAM_TEXT void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  // SysTick counts down from RVR once per 1 ms tick
  const uint32_t period = SYST_RVR + 1;
  uint64_t remaining = (uint64_t) us * (period / 1000);
  uint32_t last = SYST_CVR;
  while (remaining > 0) {
    const uint32_t now = SYST_CVR;
    const uint32_t elapsed = now <= last ? last - now : last + period - now;
    if (elapsed >= remaining)
      break;
    remaining -= elapsed;
    last = now;
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8720C
