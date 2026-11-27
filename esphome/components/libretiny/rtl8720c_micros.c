// RTL8720C: micros() and delayMicroseconds() read the free-running system timer, which keeps counting while
// interrupts are masked; LibreTiny's micros() adds SysTick to the tick count and goes back up to 1 ms after a
// missed tick. Linked with -Wl,--wrap (RTL8720C_TIME_WRAPS in __init__.py).

#ifdef USE_LIBRETINY_VARIANT_RTL8720C

#include <stdint.h>
#include "cmsis.h"
#include "hal_timer.h"

// Interrupt handlers call these, so they live in RAM like IRAM_ATTR (hal.h)
#define RTL8720C_RAM_TEXT __attribute__((noinline, section(".sram.text")))

RTL8720C_RAM_TEXT unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  // 32-bit microsecond count that wraps after 71.6 minutes, as micros() does elsewhere
  return hal_read_curtime_us();
}

RTL8720C_RAM_TEXT void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  const uint32_t start = hal_read_curtime_us();
  // <= covers the part of the first microsecond that had already passed
  while (hal_read_curtime_us() - start <= us) {
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8720C
