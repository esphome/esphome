#pragma once

#ifdef USE_LIBRETINY

#include <cstdint>

// For the inline millis() fast paths (xTaskGetTickCount, portTICK_PERIOD_MS).
#include <FreeRTOS.h>
#include <task.h>

#include "esphome/core/time_64.h"

// IRAM_ATTR places a function in executable RAM so it is callable from an
// ISR even while flash is busy (XIP stall, OTA, logger flash write).
// Each family uses a section its stock linker already routes to RAM:
// RTL8710B → .image2.ram.text, RTL8720C → .sram.text. LN882H is the
// exception: its stock linker has no matching glob, so patch_linker.py
// injects KEEP(*(.sram.text*)) into .flash_copysection at pre-link.
//
// BK72xx (all variants) are left as a no-op: their SDK wraps flash
// operations in GLOBAL_INT_DISABLE() which masks FIQ + IRQ at the CPU for
// the duration of every write, so no ISR fires while flash is stalled and
// the race IRAM_ATTR guards against cannot occur. The trade-off is that
// interrupts are delayed (not dropped) by up to ~20 ms during a sector
// erase, but that is an SDK-level choice and cannot be changed from this
// layer.
#if defined(USE_BK72XX)
#define IRAM_ATTR
#elif defined(USE_LIBRETINY_VARIANT_RTL8710B)
// Stock linker consumes *(.image2.ram.text*) into .ram_image2.text (> BD_RAM).
#define IRAM_ATTR __attribute__((noinline, section(".image2.ram.text")))
#else
// RTL8720C: stock linker consumes *(.sram.text*) into .ram.code_text.
// LN882H: patch_linker.py.script injects *(.sram.text*) into
// .flash_copysection (> RAM0 AT> FLASH).
#define IRAM_ATTR __attribute__((noinline, section(".sram.text")))
#endif
#define PROGMEM

#ifdef USE_BK72XX
// Declared in the Beken FreeRTOS port (portmacro.h) and built in ARM mode so
// it is callable from Thumb code via interworking. The MRS CPSR instruction
// is ARM-only and user code here may be built in Thumb, so in_isr_context()
// defers to this port helper on BK72xx instead of reading CPSR inline.
extern "C" uint32_t platform_is_in_interrupt_context(void);
#endif

// Forward decls from Arduino's <Arduino.h> for the inline wrappers below.
// NOLINTBEGIN(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)
extern "C" void yield(void);
extern "C" void delay(unsigned long ms);
extern "C" unsigned long micros(void);
extern "C" unsigned long millis(void);
extern "C" void delayMicroseconds(unsigned int us);
// NOLINTEND(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)

// Forward decls from libretiny's <lt_api.h> family for the inline arch_*
// wrappers below. Pulling the full header would drag in the rest of the
// LibreTiny C API.
extern "C" void lt_wdt_feed(void);
extern "C" uint32_t lt_cpu_get_cycle_count(void);
extern "C" uint32_t lt_cpu_get_freq(void);

namespace esphome::libretiny {}

namespace esphome {

/// Returns true when executing inside an interrupt handler.
__attribute__((always_inline)) inline bool in_isr_context() {
#if defined(USE_BK72XX)
  // BK72xx is ARM968E-S (ARM9); see extern declaration above.
  return platform_is_in_interrupt_context() != 0;
#else
  // Cortex-M (AmebaZ, AmebaZ2, LN882H). IPSR is the active exception number;
  // non-zero means we're in a handler.
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
#endif
}

__attribute__((always_inline)) inline void yield() { ::yield(); }
__attribute__((always_inline)) inline void delay(uint32_t ms) { ::delay(ms); }
__attribute__((always_inline)) inline uint32_t micros() { return static_cast<uint32_t>(::micros()); }

// Per-variant millis() fast path — matches MillisInternal::get().
#if defined(USE_RTL87XX) || defined(USE_LN882X)
static_assert(configTICK_RATE_HZ == 1000, "millis() fast path requires 1 kHz FreeRTOS tick");
__attribute__((always_inline)) inline uint32_t millis() {
  // xTaskGetTickCountFromISR is mandatory in interrupt context per the FreeRTOS API contract.
  return in_isr_context() ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
}
#elif defined(USE_BK72XX)
static_assert(configTICK_RATE_HZ == 500, "BK72xx millis() fast path assumes 500 Hz FreeRTOS tick");
__attribute__((always_inline)) inline uint32_t millis() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }
#else
__attribute__((always_inline)) inline uint32_t millis() { return static_cast<uint32_t>(::millis()); }
#endif
__attribute__((always_inline)) inline uint64_t millis_64() { return Millis64Impl::compute(millis()); }

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { ::delayMicroseconds(us); }
__attribute__((hot, always_inline)) inline void arch_feed_wdt() { lt_wdt_feed(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() { return lt_cpu_get_cycle_count(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_freq_hz() { return lt_cpu_get_freq(); }

void arch_init();

}  // namespace esphome

#endif  // USE_LIBRETINY
