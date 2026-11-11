#pragma once
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#ifdef __XTENSA__
#include "esp_cpu_utils.h"
#include "esp_debug_helpers.h"
#endif
#include "esp_timer.h"
// Counts every global operator new and, once armed, keeps the first ALLOC_CAP of them with five caller PCs
// (callers on Xtensa only; RISC-V entries keep size and time). Aligned operator new is not counted.
// See reference/recipes/heap_alloc.md.
struct AllocEntry {
  uint32_t ms;
  uint32_t size;  // ALLOC_MARKER for a marker: pc[0] and pc[1] then hold its two values
  uint32_t pc[5];
};
static constexpr uint32_t ALLOC_CAP = 1024;
static constexpr uint32_t ALLOC_MARKER = UINT32_MAX;
AllocEntry g_allocs[ALLOC_CAP];
std::atomic<uint32_t> g_alloc_n{0};
std::atomic<uint32_t> g_new_count{0};
// seq_cst with g_alloc_busy: the dump stores false then loads busy, a recorder adds busy then loads this
std::atomic<bool> g_alloc_armed{false};
uint32_t g_alloc_armed_ms = 0;  // when recording started, 0 if it never did
// Recorders between claiming a slot and filling it; the dump waits for 0 after disarming.
std::atomic<uint32_t> g_alloc_busy{0};

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
  g_new_count.fetch_add(1, std::memory_order_relaxed);
  g_alloc_busy.fetch_add(1);  // before the armed check, so a dump that disarmed first sees it
  AllocEntry *e = g_alloc_armed ? alloc_claim(size) : nullptr;
  if (e != nullptr) {
    e->pc[0] = e->pc[1] = e->pc[2] = e->pc[3] = e->pc[4] = 0;
#ifdef __XTENSA__
    esp_backtrace_frame_t f;
    esp_backtrace_get_start(&f.pc, &f.sp, &f.next_pc);
    // The start frame is this function; record the five callers above it.
    // Stop at the first frame that does not unwind; the slots after it stay 0.
    for (int k = 0; k < 5 && f.next_pc != 0 && esp_backtrace_get_next_frame(&f); k++)
      e->pc[k] = esp_cpu_process_stack_pc(f.pc);
#endif
  }
  g_alloc_busy.fetch_sub(1);
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
  g_alloc_busy.fetch_add(1);
  if (AllocEntry *e = g_alloc_armed ? alloc_claim(ALLOC_MARKER) : nullptr) {
    e->pc[0] = a;
    e->pc[1] = b;
    e->pc[2] = e->pc[3] = e->pc[4] = 0;
  }
  g_alloc_busy.fetch_sub(1);
}
