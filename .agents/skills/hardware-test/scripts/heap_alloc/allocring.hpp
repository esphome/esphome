#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
#include "esp_cpu_utils.h"
#include "esp_debug_helpers.h"
#include "esp_timer.h"
// Counts every global operator new and, once armed, keeps the first ALLOC_CAP of them with five caller PCs.
// See reference/recipes/heap_alloc.md.
struct AllocEntry {
  uint32_t ms;
  uint32_t size;  // 0 for a marker: pc[0] and pc[1] then hold its two values
  uint32_t pc[5];
};
static constexpr uint32_t ALLOC_CAP = 1024;
AllocEntry g_allocs[ALLOC_CAP];
std::atomic<uint32_t> g_alloc_n{0};
volatile uint32_t g_new_count = 0;
volatile bool g_alloc_armed = false;

// Claims the next slot, or returns nullptr and disarms once the buffer is full.
static inline AllocEntry *alloc_claim(uint32_t size) {
  uint32_t i = g_alloc_n.fetch_add(1);
  if (i >= ALLOC_CAP) {
    g_alloc_armed = false;
    return nullptr;
  }
  AllocEntry &e = g_allocs[i];
  e.ms = (uint32_t) (esp_timer_get_time() / 1000);
  e.size = size;
  return &e;
}

void *operator new(size_t size) {
  g_new_count++;
  AllocEntry *e = g_alloc_armed ? alloc_claim(size) : nullptr;
  if (e != nullptr) {
    esp_backtrace_frame_t f;
    esp_backtrace_get_start(&f.pc, &f.sp, &f.next_pc);
    // The start frame is this function; record the five callers above it.
    for (int k = 0; k < 5; k++)
      e->pc[k] = f.next_pc != 0 && esp_backtrace_get_next_frame(&f) ? esp_cpu_process_stack_pc(f.pc) : 0;
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

// Splits the dump into phases, for example on each change of an entity's state.
static inline void alloc_marker(uint32_t a, uint32_t b) {
  if (AllocEntry *e = g_alloc_armed ? alloc_claim(0) : nullptr) {
    e->pc[0] = a;
    e->pc[1] = b;
    e->pc[2] = e->pc[3] = e->pc[4] = 0;
  }
}
