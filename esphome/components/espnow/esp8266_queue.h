#pragma once

#ifdef USE_ESP8266

// This file provides an interrupt-safe SPSC ring buffer for ESP8266.
//
// LockFreeQueue uses std::atomic<uint8_t> / std::atomic<uint16_t>.
// On xtensa-lx106 (ESP8266), GCC emits calls to __atomic_fetch_add_2 and
// __atomic_exchange_2 for sub-word atomics, which require libatomic.
// The PlatformIO xtensa toolchain does not ship libatomic.a.
//
// On ESP8266 there is no preemptive threading; the only concurrent access is
// between the main loop and WiFi interrupt callbacks.  Disabling interrupts
// briefly is sufficient and correct for SPSC use.
//
// API is intentionally compatible with LockFreeQueue<T, SIZE> so that
// espnow_component.h can select the right type via the PacketQueue alias.

#include "esphome/core/helpers.h"

namespace esphome::espnow {

template<class T, uint8_t SIZE> class ESP8266Queue {
 public:
  bool push(T *element) {
    if (element == nullptr)
      return false;
    InterruptLock lock;
    uint8_t next_tail = next_index(tail_);
    if (next_tail == head_) {
      dropped_count_++;
      return false;
    }
    buffer_[tail_] = element;
    tail_ = next_tail;
    return true;
  }

  T *pop() {
    InterruptLock lock;
    if (head_ == tail_)
      return nullptr;
    T *element = buffer_[head_];
    head_ = next_index(head_);
    return element;
  }

  uint16_t get_and_reset_dropped_count() {
    InterruptLock lock;
    uint16_t count = dropped_count_;
    dropped_count_ = 0;
    return count;
  }

  void increment_dropped_count() {
    InterruptLock lock;
    dropped_count_++;
  }

  bool empty() const {
    InterruptLock lock;
    return head_ == tail_;
  }

  bool full() const {
    InterruptLock lock;
    return next_index(tail_) == head_;
  }

 protected:
  static constexpr uint8_t next_index(uint8_t index) {
    uint8_t next = index + 1;
    if (next >= SIZE)
      next = 0;
    return next;
  }

  T *buffer_[SIZE]{};
  uint16_t dropped_count_{0};
  uint8_t head_{0};
  uint8_t tail_{0};
};

}  // namespace esphome::espnow

#endif  // USE_ESP8266
