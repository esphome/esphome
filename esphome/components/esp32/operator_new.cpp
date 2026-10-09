/*
 * Replaces operator new for ESP-IDF builds without C++ exceptions. libstdc++'s nothrow forms call the
 * throwing form and catch std::bad_alloc, but the throw aborts here, so they abort instead of returning
 * nullptr. These forms return nullptr or abort directly, which also drops the exception class code.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#include <sdkconfig.h>
#ifndef CONFIG_COMPILER_CXX_EXCEPTIONS

#include <malloc.h>
#include <cstdlib>
#include <new>
#include "esp_system.h"

namespace esphome::esp32 {}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
  // malloc(0) may return NULL, but operator new must return a unique pointer
  if (size == 0)
    size = 1;
  for (;;) {
    void *ptr = std::malloc(size);  // NOLINT(cppcoreguidelines-no-malloc)
    if (ptr != nullptr)
      return ptr;
    std::new_handler handler = std::get_new_handler();
    if (handler == nullptr)
      return nullptr;
    handler();
  }
}

void *operator new(std::size_t size) {
  void *ptr = ::operator new(size, std::nothrow);  // NOLINT: this is the nothrow form being fixed
  if (ptr == nullptr)
    esp_system_abort("std::bad_alloc");
  return ptr;
}

void *operator new[](std::size_t size) { return ::operator new(size); }
void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept { return ::operator new(size, tag); }

// Aligned forms; libstdc++'s aligned delete frees with free(), which matches memalign()
void *operator new(std::size_t size, std::align_val_t align, const std::nothrow_t & /*tag*/) noexcept {
  if (size == 0)
    size = 1;
  for (;;) {
    void *ptr = memalign(static_cast<std::size_t>(align), size);
    if (ptr != nullptr)
      return ptr;
    std::new_handler handler = std::get_new_handler();
    if (handler == nullptr)
      return nullptr;
    handler();
  }
}

void *operator new(std::size_t size, std::align_val_t align) {
  void *ptr = ::operator new(size, align, std::nothrow);  // NOLINT: this is the nothrow form being fixed
  if (ptr == nullptr)
    esp_system_abort("std::bad_alloc");
  return ptr;
}

void *operator new[](std::size_t size, std::align_val_t align) { return ::operator new(size, align); }
void *operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t &tag) noexcept {
  return ::operator new(size, align, tag);
}

#endif  // CONFIG_COMPILER_CXX_EXCEPTIONS
#endif  // USE_ESP_IDF
