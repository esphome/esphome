/*
 * Linker wrap stubs for std::__throw_* functions.
 *
 * ESP-IDF compiles with -fno-exceptions, so C++ exceptions always abort.
 * However, ESP-IDF only wraps low-level functions (__cxa_throw, etc.),
 * not the std::__throw_* functions that construct exception objects first.
 * This pulls in ~3KB of dead exception class code that can never run.
 *
 * ESP8266 Arduino already solved this: their toolchain rebuilds libstdc++
 * with throw functions that just call abort(). We achieve the same result
 * using linker --wrap without requiring toolchain changes.
 *
 * These stubs abort immediately with a descriptive message, allowing
 * the linker to dead-code eliminate the exception class infrastructure.
 *
 * Wrapped functions and their callers:
 * - std::__throw_length_error: std::string::reserve, std::vector::reserve
 * - std::__throw_logic_error: std::promise, std::packaged_task
 * - std::__throw_out_of_range: std::string::at, std::vector::at
 * - std::__throw_out_of_range_fmt: std::bitset::to_ulong
 * - std::__throw_bad_alloc: operator new
 * - std::__throw_bad_function_call: std::function::operator()
 */

#ifdef USE_ESP_IDF
#include "esp_system.h"

namespace esphome::esp32 {}

// Linker wraps for std::__throw_* - must be extern "C" at global scope.
// Names must be __wrap_ + mangled name for the linker's --wrap option.

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

// std::__throw_length_error(char const*) - called when container size exceeds max_size()
void __wrap__ZSt20__throw_length_errorPKc(const char *) { esp_system_abort("std::length_error"); }

// std::__throw_logic_error(char const*) - called for logic errors (e.g., promise already satisfied)
void __wrap__ZSt19__throw_logic_errorPKc(const char *) { esp_system_abort("std::logic_error"); }

// std::__throw_out_of_range(char const*) - called by at() when index is out of bounds
void __wrap__ZSt20__throw_out_of_rangePKc(const char *) { esp_system_abort("std::out_of_range"); }

// std::__throw_out_of_range_fmt(char const*, ...) - called by bitset::to_ulong when value doesn't fit
void __wrap__ZSt24__throw_out_of_range_fmtPKcz(const char *, ...) { esp_system_abort("std::out_of_range"); }

// std::__throw_bad_alloc() - called when operator new fails
void __wrap__ZSt17__throw_bad_allocv() { esp_system_abort("std::bad_alloc"); }

// std::__throw_bad_function_call() - called when invoking empty std::function
void __wrap__ZSt25__throw_bad_function_callv() { esp_system_abort("std::bad_function_call"); }

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP_IDF
