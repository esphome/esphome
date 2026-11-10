#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
#include "esp_debug_helpers.h"
#include "esp_cpu_utils.h"
#include "esp_timer.h"
// Records every global operator new (size, five return PCs, time) into a ring once armed, so the
// callers can be decoded with addr2line later. Markers (size 0) record cover operation changes.
struct AllocEntry {
  uint32_t ms;
  uint32_t size;
  uint32_t pc[5];
};
static constexpr uint32_t ALLOC_RING = 1024;
AllocEntry g_alloc_ring[ALLOC_RING];
std::atomic<uint32_t> g_alloc_n{0};
volatile uint32_t g_new_count = 0;
volatile bool g_alloc_armed = false;

static inline void alloc_record(uint32_t size, const uint32_t *pcs) {
  uint32_t i = g_alloc_n.fetch_add(1);
  if (i >= ALLOC_RING)
    return;
  AllocEntry &e = g_alloc_ring[i];
  e.ms = (uint32_t) (esp_timer_get_time() / 1000);
  e.size = size;
  for (int k = 0; k < 5; k++)
    e.pc[k] = pcs[k];
}

void *operator new(size_t size) {
  g_new_count++;
  if (g_alloc_armed) {
    uint32_t pcs[5] = {0};
    esp_backtrace_frame_t f;
    esp_backtrace_get_start(&f.pc, &f.sp, &f.next_pc);
    // Frame 0 is this function; keep the five callers above it.
    for (int k = -1; k < 5; k++) {
      if (k >= 0)
        pcs[k] = esp_cpu_process_stack_pc(f.pc);
      if (f.next_pc == 0 || !esp_backtrace_get_next_frame(&f))
        break;
    }
    alloc_record(size, pcs);
  }
  void *p = malloc(size ? size : 1);
  if (p == nullptr)
    abort();
  return p;
}
void *operator new[](size_t size) { return operator new(size); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }

static inline void alloc_marker(uint32_t op, uint32_t pos_milli) {
  uint32_t pcs[5] = {op, pos_milli, 0, 0, 0};
  alloc_record(0, pcs);
}
