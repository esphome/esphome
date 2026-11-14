#pragma once
// Main task stack high-water mark without uxTaskGetStackHighWaterMark (compiled out on LibreTiny's RTL8720C and
// BK7231 kernels): count the untouched fill bytes from the stack base, as FreeRTOS itself does.
// pxStack is the second TCB member on ARMv8-M builds with V8M_STKOVF (RTL8720C) and the member right before
// pcTaskName otherwise; the first call takes the candidate that sits below the current stack pointer and starts with
// a fill byte, and later calls reuse it. Returns -1 when no candidate qualifies on the first call (unknown base, never
// a measurement) and 0 when the known base no longer holds the fill byte (the stack was used to its end).
#include <FreeRTOS.h>
#include <task.h>
#include <cstdint>

static inline int32_t stackmin_free_bytes() {
  static uint8_t *s_base = nullptr;
  static uint8_t s_fill = 0;
  volatile uint8_t here = 0;
  uintptr_t sp = (uintptr_t) &here;
  if (s_base == nullptr) {
    uint8_t *tcb = (uint8_t *) xTaskGetCurrentTaskHandle();
    uint8_t *name = (uint8_t *) pcTaskGetName(nullptr);
    uint8_t *cands[2] = {*(uint8_t **) (tcb + 4), *(uint8_t **) (name - sizeof(void *))};
    for (uint8_t *base : cands) {
      if (base == nullptr || (uintptr_t) base >= sp || sp - (uintptr_t) base > 65536)
        continue;
      if (base[0] != 0xa5 && base[0] != 0x5a)
        continue;
      s_base = base;
      s_fill = base[0];
      break;
    }
    if (s_base == nullptr)
      return -1;
  }
  int32_t n = 0;
  while ((uintptr_t) (s_base + n) < sp && s_base[n] == s_fill)
    n++;
  return n;
}
