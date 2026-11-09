#pragma once

#include "FreeRTOS.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <vector>

// This double records admission/wait arguments, not real-time scheduling.
// ESP-IDF's BYTEBUF rejects oversized sends before entering its wait path.
struct StaticRingbuffer_t {
  uint8_t *storage;
  size_t capacity;
  size_t read;
  size_t write;
  size_t used;
  size_t leased;
};
using RingbufHandle_t = StaticRingbuffer_t *;
enum RingbufferType_t { RINGBUF_TYPE_BYTEBUF };

namespace ringbuf_test {
struct Send {
  size_t length;
  TickType_t ticks;
};
inline std::vector<Send> sends;
inline std::vector<TickType_t> waits;
inline std::function<void()> on_wait;
inline unsigned free_queries = 0;
inline void reset() {
  sends.clear();
  waits.clear();
  on_wait = {};
  free_queries = 0;
}
}  // namespace ringbuf_test

inline RingbufHandle_t xRingbufferCreateStatic(size_t size, RingbufferType_t, uint8_t *storage,
                                               StaticRingbuffer_t *state) {
  *state = {storage, size, 0, 0, 0, 0};
  return state;
}
inline void vRingbufferDelete(RingbufHandle_t) {}

inline BaseType_t xRingbufferSend(RingbufHandle_t state, const void *data, size_t length, TickType_t ticks) {
  ringbuf_test::sends.push_back({length, ticks});
  if (length > state->capacity)
    return 0;
  if (length > state->capacity - state->used && ticks != 0) {
    ringbuf_test::waits.push_back(ticks);
    if (ringbuf_test::on_wait)
      ringbuf_test::on_wait();
  }
  if (length > state->capacity - state->used)
    return 0;
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < length; ++i) {
    state->storage[state->write] = bytes[i];
    state->write = (state->write + 1) % state->capacity;
  }
  state->used += length;
  return pdPASS;
}
inline void *xRingbufferReceiveUpTo(RingbufHandle_t state, size_t *length, TickType_t, size_t maximum) {
  assert(state->leased == 0);
  *length = std::min({maximum, state->used, state->capacity - state->read});
  state->leased = *length;
  return *length ? state->storage + state->read : nullptr;
}
inline void vRingbufferReturnItem(RingbufHandle_t state, void *item) {
  assert(item == state->storage + state->read);
  state->read = (state->read + state->leased) % state->capacity;
  state->used -= state->leased;
  state->leased = 0;
}
inline size_t xRingbufferGetCurFreeSize(RingbufHandle_t state) {
  ++ringbuf_test::free_queries;
  return state->capacity - state->used;
}
inline void vRingbufferGetInfo(RingbufHandle_t state, UBaseType_t *, UBaseType_t *, UBaseType_t *, UBaseType_t *,
                               UBaseType_t *waiting) {
  *waiting = state->used - state->leased;
}
