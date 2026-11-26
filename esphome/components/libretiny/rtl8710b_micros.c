// RTL8710B: the FreeRTOS tick is 1/1024 s, so millis(), micros() and delay() use the 32.768 kHz system timer instead
// and delayMicroseconds() the DWT cycle counter. Linked with -Wl,--wrap (RTL8710B_TIME_WRAPS in __init__.py).

#ifdef USE_LIBRETINY_VARIANT_RTL8710B

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

extern uint32_t SystemCoreClock;
__attribute__((long_call)) uint32_t SYSTIMER_TickGet(void);  // mask ROM, rtl8710b_delay.h

#define DEMCR (*(volatile uint32_t *) 0xE000EDFC)
#define DEMCR_TRCENA (1UL << 24)
#define DWT_CTRL (*(volatile uint32_t *) 0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *) 0xE0001004)
// Interrupt handlers call these, so they live in RAM like IRAM_ATTR (hal.h)
#define RTL8710B_RAM_TEXT __attribute__((noinline, section(".sram.text")))

// Extends the 32-bit timer, which wraps every 36.4 hours; the main loop calls millis() far more often
static inline __attribute__((always_inline)) uint64_t timer_ticks(void) {
  static uint64_t ticks;
  uint32_t primask;
  __asm volatile("mrs %0, primask\n cpsid i" : "=r"(primask)::"memory");
  ticks += (uint32_t) (SYSTIMER_TickGet() - (uint32_t) ticks);
  const uint64_t now = ticks;
  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
  return now;
}

RTL8710B_RAM_TEXT unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  // 1000000 / 32768 = 15625 / 512
  return (uint32_t) ((timer_ticks() * 15625) >> 9);
}

RTL8710B_RAM_TEXT unsigned long __wrap_millis(void) {  // NOLINT(readability-identifier-naming)
  // 1000 / 32768 = 125 / 4096
  return (uint32_t) ((timer_ticks() * 125) >> 12);
}

void __wrap_delay(uint32_t ms) {  // NOLINT(readability-identifier-naming)
  if (ms == 0) {
    vTaskDelay(0);
    return;
  }
  // vTaskDelay() counts the partial tick, so sleep to a deadline; +1 covers the elapsed part of the current count
  const uint64_t deadline = timer_ticks() + ((uint64_t) ms * 32768 + 999) / 1000 + 1;
  // Sleeps are capped at 1024 s so a delay longer than the 36.4 hour timer wrap still sees every wrap
  for (uint64_t now = timer_ticks(); now < deadline; now = timer_ticks()) {
    const uint64_t ticks = (deadline - now + 31) / 32;
    vTaskDelay((TickType_t) (ticks < 0x100000 ? ticks : 0x100000));
  }
}

RTL8710B_RAM_TEXT void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  DEMCR |= DEMCR_TRCENA;
  DWT_CTRL |= 1;
  uint32_t start = DWT_CYCCNT;
  // Rounded up: the BW12 runs at 62.5 MHz
  uint64_t cycles = (uint64_t) us * ((SystemCoreClock + 999999) / 1000000);
  // Steps of at most 2^31 cycles keep the 32-bit difference from wrapping
  while (cycles > 0) {
    const uint32_t step = cycles > 0x80000000UL ? 0x80000000UL : (uint32_t) cycles;
    while (DWT_CYCCNT - start < step) {
    }
    start += step;
    cycles -= step;
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8710B
