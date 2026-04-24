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

#else

#define IRAM_ATTR
#define PROGMEM

#endif

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#ifdef USE_BK72XX
// Declared in the Beken FreeRTOS port (portmacro.h) and built in ARM mode so
// it is callable from Thumb code via interworking. The MRS CPSR instruction
// is ARM-only and user code here may be built in Thumb, so in_isr_context()
// defers to this port helper on BK72xx instead of reading CPSR inline.
extern "C" uint32_t platform_is_in_interrupt_context(void);
#endif

namespace esphome {

/// Returns true when executing inside an interrupt handler.
/// always_inline so callers placed in IRAM keep the detection in IRAM.
__attribute__((always_inline)) inline bool in_isr_context() {
#if defined(USE_ESP32)
  return xPortInIsrContext() != 0;
#elif defined(USE_ESP8266)
  // ESP8266 has no reliable single-register ISR detection: PS.INTLEVEL is
  // non-zero both in a real ISR and when user code masks interrupts.  The
  // ESP8266 wake path is context-agnostic (wake_loop_impl uses esp_schedule
  // which is ISR-safe) so this helper is unused on this platform.
  return false;
#elif defined(USE_RP2040)
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
#elif defined(USE_BK72XX)
  // BK72xx is ARM968E-S (ARM9); see extern declaration above.
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
