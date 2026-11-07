/*
 * RTL8710B: SysTick runs from the 32 kHz clock (reload 31 for the 1 ms tick), but LibreTiny's micros() scales the
 * counter by the CPU clock, so the part below 1 ms is always 0 and micros() moves in 1 ms steps. Its
 * delayMicroseconds() waits on that micros() above 100 us, so every such delay takes about 1 ms. Linked with
 * -Wl,--wrap=micros and -Wl,--wrap=delayMicroseconds so every caller outside LibreTiny's wiring.c gets these.
 */

#ifdef USE_LIBRETINY_VARIANT_RTL8710B

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>

extern uint32_t SystemCoreClock;

#define SYST_RVR (*(volatile uint32_t *) 0xE000E014)
#define SYST_CVR (*(volatile uint32_t *) 0xE000E018)
#define SCB_ICSR (*(volatile uint32_t *) 0xE000ED04)
#define ICSR_PENDSTSET (1UL << 26)
#define DEMCR (*(volatile uint32_t *) 0xE000EDFC)
#define DEMCR_TRCENA (1UL << 24)
#define DWT_CTRL (*(volatile uint32_t *) 0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *) 0xE0001004)

static uint32_t tick_count(uint32_t in_isr) { return in_isr ? xTaskGetTickCountFromISR() : xTaskGetTickCount(); }

unsigned long __wrap_micros(void) {  // NOLINT(readability-identifier-naming)
  uint32_t in_isr;
  __asm volatile("mrs %0, ipsr" : "=r"(in_isr));
  uint32_t tick, cur, pending;
  do {
    tick = tick_count(in_isr);
    pending = SCB_ICSR & ICSR_PENDSTSET;
    cur = SYST_CVR;
    // The tick came due between the two reads: read the counter again after it
    if (!pending && (SCB_ICSR & ICSR_PENDSTSET)) {
      pending = 1;
      cur = SYST_CVR;
    }
  } while (tick != tick_count(in_isr));
  // A tick that is due but not counted yet (interrupts masked, or called from a higher priority interrupt)
  if (pending)
    tick++;
  // The tick comes due when the counter reaches 0, so 0 starts the millisecond and 1 ends it
  const uint32_t reload = SYST_RVR;
  const uint32_t elapsed = cur == 0 ? 0 : reload + 1 - cur;
  return tick * 1000 + elapsed * 1000 / (reload + 1);
}

void __wrap_delayMicroseconds(unsigned int us) {  // NOLINT(readability-identifier-naming)
  DEMCR |= DEMCR_TRCENA;
  DWT_CTRL |= 1;
  const uint32_t start = DWT_CYCCNT;
  const uint32_t cycles = us * (SystemCoreClock / 1000000);
  while (DWT_CYCCNT - start < cycles) {
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8710B
