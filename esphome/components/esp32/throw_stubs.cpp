/*
 * Linker wrap stubs for std::__throw_* functions.
 *
 * ESP-IDF disables C++ exceptions but doesn't wrap the std::__throw_*
 * functions that construct exception objects before calling __cxa_throw.
 * This wastes ~1KB on exception class code that can never execute.
 *
 * These stubs abort immediately with a descriptive message, allowing
 * the linker to dead-code eliminate the exception class infrastructure.
 *
 * The ESP8266 Arduino toolchain solves this by rebuilding libstdc++ with
 * similar stubs. We achieve the same result using linker --wrap.
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

extern "C" {

// std::__throw_length_error(char const*)
// Called when container size exceeds max_size()
void __wrap__ZSt20__throw_length_errorPKc(const char *) { esp_system_abort("std::length_error"); }

// std::__throw_logic_error(char const*)
// Called for logic errors (e.g., promise already satisfied)
void __wrap__ZSt19__throw_logic_errorPKc(const char *) { esp_system_abort("std::logic_error"); }

// std::__throw_out_of_range(char const*)
// Called by at() when index is out of bounds
void __wrap__ZSt20__throw_out_of_rangePKc(const char *) { esp_system_abort("std::out_of_range"); }

// std::__throw_out_of_range_fmt(char const*, ...)
// Called by bitset::to_ulong when value doesn't fit
void __wrap__ZSt24__throw_out_of_range_fmtPKcz(const char *, ...) { esp_system_abort("std::out_of_range"); }

// std::__throw_bad_alloc()
// Called when operator new fails
void __wrap__ZSt17__throw_bad_allocv() { esp_system_abort("std::bad_alloc"); }

// std::__throw_bad_function_call()
// Called when invoking empty std::function
void __wrap__ZSt25__throw_bad_function_callv() { esp_system_abort("std::bad_function_call"); }

}  // extern "C"
#endif  // USE_ESP_IDF
