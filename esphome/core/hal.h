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

// IRAM_ATTR places a function in SRAM so it is callable from an ISR even
// while flash is busy (XIP stall, OTA, logger flash write). The section used
// varies per family, based on what each linker script already supports:
// - RTL8710B (AmebaZ): ".image2.ram.text" output section exists.
// - RTL8720C (AmebaZ2): "*(.sram.text*)" is already consumed.
// - BK72xx / LN882H: stock linker script has no RAM text section, so we
//   piggyback on ".data" — the SDK startup code copies .data from flash into
//   SRAM before main() runs, so the function body ends up in executable RAM.
#if defined(USE_LIBRETINY_VARIANT_RTL8710B)
#define IRAM_ATTR __attribute__((noinline, section(".image2.ram.text")))
#elif defined(USE_LIBRETINY_VARIANT_RTL8720C)
#define IRAM_ATTR __attribute__((noinline, section(".sram.text")))
#else
#define IRAM_ATTR __attribute__((noinline, section(".data")))
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
  // BK72xx is ARM968E-S (ARM9). CPSR mode bits [4:0]:
  // 0x10 USR, 0x13 SVC, 0x1F SYS are normal; others (IRQ=0x12, FIQ=0x11,
  // ABT=0x17, UND=0x1B) are exception modes.
  uint32_t cpsr;
  __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
  uint32_t mode = cpsr & 0x1Fu;
  return mode != 0x10u && mode != 0x13u && mode != 0x1Fu;
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
