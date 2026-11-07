/*
 * RTL8710B: the FreeRTOS tick is 32 periods of the 32.768 kHz clock (SysTick reload 31), so a "1 ms" tick is
 * 976.6 us and the tick count runs 2.4% fast. LibreTiny's micros() also scales the SysTick counter by the CPU clock,
 * so it moves in whole ticks, and its delayMicroseconds() waits on that micros() above 100 us.
 *
 * millis(), micros() and delay() here run on the free running 32.768 kHz system timer (TIM0) instead, which keeps
 * counting while interrupts are masked, the scheduler is suspended or the tick is stopped in tickless idle.
 * delayMicroseconds() waits on the CPU cycle counter. Linked with -Wl,--wrap for each, so every caller outside
 * LibreTiny's own wiring.c gets these.
 */

#ifdef USE_LIBRETINY_VARIANT_RTL8710B

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

extern uint32_t SystemCoreClock;
uint32_t SYSTIMER_TickGet(void);  // mask ROM, rtl8710b_delay.h

#define DEMCR (*(volatile uint32_t *) 0xE000EDFC)
#define DEMCR_TRCENA (1UL << 24)
#define DWT_CTRL (*(volatile uint32_t *) 0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *) 0xE0001004)

// The 32-bit timer wraps every 36.4 hours; counting the wraps needs a call in between, which the main loop's
// millis() calls always give.
static uint64_t timer_ticks(void) {
  static uint32_t last;
  static uint32_t wraps;
  uint32_t primask;
  __asm volatile("mrs %0, primask\n cpsid i" : "=r"(primask)::"memory");
  const uint32_t now = SYSTIMER_TickGet();
  if (now < last)
    wraps++;
  last = now;
  const uint64_t ticks = ((uint64_t) wraps << 32) | now;
  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
  return ticks;
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
