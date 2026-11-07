/*
 * RTL8710B: the FreeRTOS tick is 32/32768 s, so tick based millis() runs 2.4% fast and micros() moves in ticks.
 * millis(), micros() and delay() use the free running 32.768 kHz system timer (TIM0), which keeps counting with
 * interrupts masked, the scheduler suspended or the tick stopped in tickless idle; delayMicroseconds() uses the DWT
 * cycle counter. Linked with -Wl,--wrap, so every caller outside LibreTiny's wiring.c gets these.
 */

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

// The 32-bit timer wraps every 36.4 hours; extending it needs a call in between, which the main loop's millis() calls
// always give.
static inline __attribute__((always_inline)) uint64_t timer_ticks(void) {
  static uint64_t ticks;
  uint32_t primask;
  __asm volatile("mrs %0, primask\n cpsid i" : "=r"(primask)::"memory");
  ticks += (uint32_t) (SYSTIMER_TickGet() - (uint32_t) ticks);
  const uint64_t now = ticks;
  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
  return now;
}

unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  // 1000000 / 32768 = 15625 / 512
  return (uint32_t) ((timer_ticks() * 15625) >> 9);
}

unsigned long __wrap_millis(void) {  // NOLINT(readability-identifier-naming)
  // 1000 / 32768 = 125 / 4096
  return (uint32_t) ((timer_ticks() * 125) >> 12);
}

void __wrap_delay(uint32_t ms) {  // NOLINT(readability-identifier-naming)
  // A tick is 1/1024 s: wait enough whole ticks to cover ms
  vTaskDelay((TickType_t) (((uint64_t) ms * 1024 + 999) / 1000));
}

void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  DEMCR |= DEMCR_TRCENA;
  DWT_CTRL |= 1;
  uint32_t start = DWT_CYCCNT;
  // Round up so a clock that is not a whole number of MHz (62.5 MHz on the BW12) never shortens a delay
  uint64_t cycles = (uint64_t) us * ((SystemCoreClock + 999999) / 1000000);
  // Wait in steps of at most 2^31 cycles, so the 32-bit counter difference never wraps
  while (cycles > 0) {
    const uint32_t step = cycles > 0x80000000UL ? 0x80000000UL : (uint32_t) cycles;
    while (DWT_CYCCNT - start < step) {
    }
    start += step;
    cycles -= step;
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8710B
