#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome::esp_audio_stack {

/// RAII guard around a FreeRTOS mutex / counting semaphore handle, with an
/// optional acquire timeout.
///
/// Why this exists: ESPHome ships `esphome::Mutex` + `esphome::LockGuard`,
/// but `LockGuard` always acquires with `portMAX_DELAY`. Our audio paths
/// need bounded waits (e.g. 2 ms in the AEC reference path) so they can
/// gracefully skip a frame instead of stalling the audio task. ScopedLock
/// covers both cases:
///  - Default constructor (no timeout) blocks until the handle is taken,
///    matching `LockGuard` semantics.
///  - With a `timeout_ticks` argument it returns immediately on timeout
///    and the caller checks success via the bool conversion / `is_locked()`.
///
/// Usage:
/// \code
///   esp_audio_stack::ScopedLock lock(this->spk_ref_mutex_, pdMS_TO_TICKS(2));
///   if (lock) {
///     // protected section, auto-released on scope exit
///   }
/// \endcode
class ScopedLock {
 public:
  /// Take the handle with `portMAX_DELAY`. Behaves like esphome::LockGuard.
  explicit ScopedLock(SemaphoreHandle_t handle)
      : handle_(handle), locked_(handle != nullptr && xSemaphoreTake(handle, portMAX_DELAY) == pdTRUE) {}

  /// Take the handle with the given FreeRTOS tick timeout. On timeout the
  /// guard is constructed but `locked_` stays false; destructor is a noop.
  ScopedLock(SemaphoreHandle_t handle, TickType_t timeout_ticks)
      : handle_(handle), locked_(handle != nullptr && xSemaphoreTake(handle, timeout_ticks) == pdTRUE) {}

  ~ScopedLock() {
    if (this->locked_)
      xSemaphoreGive(this->handle_);
  }

  ScopedLock(const ScopedLock &) = delete;
  ScopedLock &operator=(const ScopedLock &) = delete;
  ScopedLock(ScopedLock &&) = delete;
  ScopedLock &operator=(ScopedLock &&) = delete;

  /// Returns true if the handle was successfully taken.
  explicit operator bool() const { return this->locked_; }
  bool is_locked() const { return this->locked_; }

 private:
  SemaphoreHandle_t handle_;
  bool locked_;
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
