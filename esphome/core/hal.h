#pragma once
#include <string>
#include <cstdint>
#include "gpio.h"

#if defined(USE_ESP32)
#include <esp_attr.h>
#ifndef PROGMEM
#define PROGMEM
#endif

#elif defined(USE_ESP8266)

#include <c_types.h>
#ifndef PROGMEM
#define PROGMEM ICACHE_RODATA_ATTR
#endif

#elif defined(USE_RP2040)

#define IRAM_ATTR __attribute__((noinline, long_call, section(".time_critical")))
#define PROGMEM

#elif defined(USE_LIBRETINY)

// IRAM_ATTR places a function in executable RAM so it is callable from an
// ISR even while flash is busy (XIP stall, OTA, logger flash write).
// patch_linker.py.script routes ".sram.text" into each family's RAM-
// executable output section: .itcm.code on BK7231N, .image2.ram.text on
// RTL8710B, .flash_copysection on LN882H, stock *(.sram.text*) glob on
// RTL8720C.
//
// BK7231T/Q/7251 are left as a no-op: their SDK wraps flash operations in
// GLOBAL_INT_DISABLE() which masks FIQ + IRQ for the duration of the
// write, so no ISR fires while flash is stalled and the scenario
// IRAM_ATTR guards against does not occur there.
#if defined(USE_LIBRETINY_VARIANT_BK7231T) || defined(USE_LIBRETINY_VARIANT_BK7231Q) || \
    defined(USE_LIBRETINY_VARIANT_BK7251)
#define IRAM_ATTR
#else
#define IRAM_ATTR __attribute__((noinline, section(".sram.text")))
#endif
#define PROGMEM

#else

#define IRAM_ATTR
#define PROGMEM

#endif

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#if defined(USE_LIBRETINY_VARIANT_BK7231N) || defined(USE_LIBRETINY_VARIANT_BK7231T) || \
    defined(USE_LIBRETINY_VARIANT_BK7231Q) || defined(USE_LIBRETINY_VARIANT_BK7251)
// Declared in the Beken FreeRTOS port (portmacro.h) and built in ARM mode so
// it is callable from Thumb code via interworking.
extern "C" uint32_t platform_is_in_interrupt_context(void);
#endif

namespace esphome {

/// Returns true when executing inside an interrupt handler.
/// always_inline so callers placed in IRAM keep the detection in IRAM.
__attribute__((always_inline)) inline bool in_isr_context() {
#if defined(USE_ESP32)
  return xPortInIsrContext() != 0;
#elif defined(USE_ESP8266)
  // Xtensa LX106 PS.INTLEVEL[3:0]. Non-zero indicates interrupt in progress.
  uint32_t ps;
  __asm__ volatile("rsr.ps %0" : "=r"(ps));
  return (ps & 0xF) != 0;
#elif defined(USE_RP2040)
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
#elif defined(USE_LIBRETINY_VARIANT_BK7231N) || defined(USE_LIBRETINY_VARIANT_BK7231T) || \
    defined(USE_LIBRETINY_VARIANT_BK7231Q) || defined(USE_LIBRETINY_VARIANT_BK7251)
  // BK72xx is ARM968E-S (ARM9). The MRS CPSR instruction is ARM-only, and
  // user code here may be built in Thumb mode. Defer to the FreeRTOS port
  // helper declared above (compiled in ARM mode by the SDK).
  return platform_is_in_interrupt_context() != 0;
#elif defined(USE_LIBRETINY)
  // Cortex-M (AmebaZ, AmebaZ2, LN882H). IPSR is the active exception number;
  // non-zero means we're in a handler.
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
#else
  // Host and any future platform without an ISR concept.
  return false;
#endif
}

void yield();
uint32_t millis();
uint64_t millis_64();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);  // NOLINT(readability-identifier-naming)
void __attribute__((noreturn)) arch_restart();
void arch_init();
void arch_feed_wdt();
uint32_t arch_get_cpu_cycle_count();
uint32_t arch_get_cpu_freq_hz();

#ifdef USE_ESP8266
// ESP8266: pgm_read_* does real flash reads on Harvard architecture
uint8_t progmem_read_byte(const uint8_t *addr);
const char *progmem_read_ptr(const char *const *addr);
uint16_t progmem_read_uint16(const uint16_t *addr);
#else
// All other platforms: PROGMEM is a no-op, so these are direct dereferences
inline uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
inline const char *progmem_read_ptr(const char *const *addr) { return *addr; }
inline uint16_t progmem_read_uint16(const uint16_t *addr) { return *addr; }
#endif

}  // namespace esphome
