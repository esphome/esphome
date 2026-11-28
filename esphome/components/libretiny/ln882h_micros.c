// LN882H: LibreTiny's micros() adds SysTick to the tick count and goes back up to 1 ms after a tick missed while
// interrupts are masked, so micros() and delayMicroseconds() count a free-running 1 MHz hardware timer (TIMER2; the
// SDK uses TIMER3) instead. Its 24-bit count wraps every 16.8 s, so the FreeRTOS tick hook reads it on every tick.
// Linked with -Wl,--wrap (TIME_WRAPS in __init__.py).

#ifdef USE_LIBRETINY_VARIANT_LN882H

#include <stdint.h>
#include <FreeRTOS.h>
#include "hal/hal_clock.h"
#include "hal/hal_timer.h"

#if !configUSE_TICK_HOOK
#error "micros() needs the FreeRTOS tick hook to see every 24-bit timer wrap"
#endif

#define US_TIMER TIMER2_BASE
#define US_TIMER_MASK 0xFFFFFFUL
// Interrupt handlers call these, so they live in RAM like IRAM_ATTR (hal.h)
#define LN882H_RAM_TEXT __attribute__((noinline, section(".sram.text")))

void __real_vApplicationTickHook(void);  // NOLINT(readability-identifier-naming)

static uint32_t last_count;
static uint32_t now_us;

// Before any C++ static constructor that might read micros()
__attribute__((constructor(101))) static void start_us_timer(void) {
  tim_init_t_def init = {
      .tim_mode = TIM_FREE_RUNNING_MODE,
      .tim_load_value = US_TIMER_MASK,
      .tim_div = (uint8_t) (hal_clock_get_apb0_clk() / 1000000 - 1),
  };
  hal_tim_init(US_TIMER, &init);
  hal_tim_en(US_TIMER, HAL_ENABLE);
  last_count = timer_timercurrentvalueregister_getf(US_TIMER);
}

LN882H_RAM_TEXT unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const uint32_t count = timer_timercurrentvalueregister_getf(US_TIMER);
  // The timer counts down
  now_us += (last_count - count) & US_TIMER_MASK;
  last_count = count;
  const uint32_t us = now_us;
  __set_PRIMASK(primask);
  return us;
}

// Not more than 16.8 s may pass between two reads of the 24-bit count
void __wrap_vApplicationTickHook(void) {  // NOLINT(readability-identifier-naming)
  __real_vApplicationTickHook();
  __wrap_micros();
}

LN882H_RAM_TEXT void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  const uint32_t start = __wrap_micros();
  // <= covers the part of the first microsecond that had already passed
  while (__wrap_micros() - start <= us) {
  }
}

#endif  // USE_LIBRETINY_VARIANT_LN882H
