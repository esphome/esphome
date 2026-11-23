#pragma once
// Heap allocation counter for test harnesses: counts while alloc_count::window is set. Link flag per platform:
// ESP32 and ESP8266 -Wl,--wrap=malloc (on ESP8266 the wrapper must be in IRAM: in flash it crash-loops with
// IllegalInst at its first instruction). arduino-pico already wraps malloc and _malloc_r (a second wrap is a duplicate
// symbol), so use -Wl,--wrap=__malloc_lock (malloc and free each count once). LibreTiny already wraps malloc into
// pvPortMalloc, so use -Wl,--wrap=pvPortMalloc.
#include <cstddef>
#include <cstdint>
#include "esphome/core/hal.h"

namespace alloc_count {
inline volatile bool window = false;
inline volatile uint32_t count = 0;
inline void note() {
  if (window)
    count = count + 1;
}
}  // namespace alloc_count

#if defined(USE_RP2)
extern "C" void __real___malloc_lock(struct _reent *r);
extern "C" void __wrap___malloc_lock(struct _reent *r) {
  alloc_count::note();
  __real___malloc_lock(r);
}
#elif defined(USE_LIBRETINY)
extern "C" void *__real_pvPortMalloc(size_t n);
extern "C" void *__wrap_pvPortMalloc(size_t n) {
  alloc_count::note();
  return __real_pvPortMalloc(n);
}
#elif defined(USE_ESP8266)
extern "C" void *__real_malloc(size_t n);
extern "C" void *IRAM_ATTR __wrap_malloc(size_t n) {
  if (alloc_count::window)
    alloc_count::count = alloc_count::count + 1;
  return __real_malloc(n);
}
#else
extern "C" void *__real_malloc(size_t n);
extern "C" void *__wrap_malloc(size_t n) {
  alloc_count::note();
  return __real_malloc(n);
}
#endif
