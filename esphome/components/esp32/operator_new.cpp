/*
 * Replaces operator new for ESP-IDF builds without C++ exceptions. libstdc++'s nothrow forms call the
 * throwing form and catch std::bad_alloc, but the throw aborts here, so they abort instead of returning
 * nullptr. All forms stay in this file: --undefined=_Znwj pulls it in ahead of libstdc++.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#include <sdkconfig.h>
#ifndef CONFIG_COMPILER_CXX_EXCEPTIONS

#include <cstdlib>
#include <new>
#include <type_traits>
#include "esp_system.h"

#ifdef __cpp_exceptions
#error "CONFIG_COMPILER_CXX_EXCEPTIONS no longer matches -fexceptions"
#endif

// libstdc++'s aligned delete calls free() only when built with one of these
#if !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC) && !defined(_GLIBCXX_HAVE_POSIX_MEMALIGN) && !defined(_GLIBCXX_HAVE_MEMALIGN)
#error "libstdc++'s aligned operator delete no longer frees with free()"
#endif

#ifndef CLANG_TIDY  // clang-tidy parses Xtensa with a 64 bit host target
// --undefined=_Znwj in __init__.py relies on this mangling
static_assert(std::is_same_v<std::size_t, unsigned int>, "update --undefined=_Znwj");
#endif

namespace esphome::esp32 {}  // namespace esphome::esp32

namespace {

// Standard new_handler retry loop; returns nullptr once no handler is installed
template<typename Alloc> void *alloc_or_null(std::size_t size, Alloc alloc) {
  // malloc(0) may return NULL, but operator new must return a unique pointer
  if (size == 0)
    size = 1;
  for (;;) {
    void *ptr = alloc(size);
    if (ptr != nullptr)
      return ptr;
    std::new_handler handler = std::get_new_handler();
    if (handler == nullptr)
      return nullptr;
    handler();
  }
}

void *abort_if_null(void *ptr) {
  if (ptr == nullptr)
    esp_system_abort("std::bad_alloc");
  return ptr;
}

void *plain_alloc(std::size_t size) {
  return alloc_or_null(size, [](std::size_t n) { return std::malloc(n); });  // NOLINT(cppcoreguidelines-no-malloc)
}

// IDF's aligned_alloc accepts any size (no C11 multiple of alignment rule) and pairs with free(),
// which libstdc++'s aligned delete uses (checked above)
void *aligned_alloc_or_null(std::size_t size, std::align_val_t align) {
  return alloc_or_null(size, [align](std::size_t n) { return ::aligned_alloc(static_cast<std::size_t>(align), n); });
}

}  // namespace

// libstdc++'s operator delete (free) already matches these
// NOLINTBEGIN(cert-dcl54-cpp,misc-new-delete-overloads)
void *operator new(std::size_t size) { return abort_if_null(plain_alloc(size)); }
void *operator new[](std::size_t size) { return abort_if_null(plain_alloc(size)); }
void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept { return plain_alloc(size); }
void *operator new[](std::size_t size, const std::nothrow_t & /*tag*/) noexcept { return plain_alloc(size); }

void *operator new(std::size_t size, std::align_val_t align) {
  return abort_if_null(aligned_alloc_or_null(size, align));
}
void *operator new[](std::size_t size, std::align_val_t align) {
  return abort_if_null(aligned_alloc_or_null(size, align));
}
void *operator new(std::size_t size, std::align_val_t align, const std::nothrow_t & /*tag*/) noexcept {
  return aligned_alloc_or_null(size, align);
}
void *operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t & /*tag*/) noexcept {
  return aligned_alloc_or_null(size, align);
}
// NOLINTEND(cert-dcl54-cpp,misc-new-delete-overloads)

#endif  // CONFIG_COMPILER_CXX_EXCEPTIONS
#endif  // USE_ESP_IDF
