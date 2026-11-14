#pragma once
// Main task stack high-water mark without uxTaskGetStackHighWaterMark (compiled out on LibreTiny's RTL8720C and
// BK7231 kernels): count the untouched fill bytes from the stack base, as FreeRTOS itself does.
// pxStack is the second TCB member on ARMv8-M builds with V8M_STKOVF (RTL8720C) and the member right before
// pcTaskName otherwise; take the candidate that sits below the current stack pointer.
#include <FreeRTOS.h>
#include <task.h>
#include <cstdint>

static inline int32_t stackmin_free_bytes() {
  volatile uint8_t here = 0;
  uintptr_t sp = (uintptr_t) &here;
  uint8_t *tcb = (uint8_t *) xTaskGetCurrentTaskHandle();
  uint8_t *name = (uint8_t *) pcTaskGetName(nullptr);
  uint8_t *cands[2] = {*(uint8_t **) (tcb + 4), *(uint8_t **) (name - sizeof(void *))};
  for (uint8_t *base : cands) {
    if (base == nullptr || (uintptr_t) base >= sp || sp - (uintptr_t) base > 65536)
      continue;
    uint8_t fill = base[0];
    if (fill != 0xa5 && fill != 0x5a)
      return 0;  // the deepest byte is used: no headroom left
    int32_t n = 0;
    while ((uintptr_t) (base + n) < sp && base[n] == fill)
      n++;
    return n;
  }
  return -1;  // no plausible stack base
}
