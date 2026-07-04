#pragma once
/*
 * Inline overrides for std::__throw_* helpers (ESP8266).
 *
 * ESP8266 Arduino compiles with -fno-exceptions and ships a libstdc++ whose
 * std::__throw_* functions already just call abort() -- they never read their
 * const char* message argument. But the compiler still emits the message load
 * at every throw site (inside header-instantiated std::string / std::vector
 * code), so --gc-sections keeps those libstdc++ error strings alive. On
 * ESP8266 .rodata lives in DRAM, so each one wastes scarce RAM (e.g.
 * "basic_string::_M_construct null not valid", "basic_string::_M_create",
 * "cannot create std::vector larger than max_size()", "array::at: ...").
 *
 * Providing inline definitions here lets GCC see the message argument is
 * unused, dead-strip the load, and drop the string entirely -- no LTO needed.
 * Behavior is identical to today: a bare abort() (the message was never
 * printed). This header MUST be force-included before <string>, so it is
 * wired up via build_src_flags "-include ..." in this component's __init__.py.
 *
 * Note: this defines functions in namespace std (technically UB). It is safe
 * here because the definitions match the existing abort() behavior exactly.
 */

#ifdef __cplusplus

// Empty namespace so the CI namespace check is satisfied; the overrides below
// must live in namespace std, so they cannot go in the component namespace.
namespace esphome::esp8266 {}  // namespace esphome::esp8266

// NOLINTBEGIN(bugprone-reserved-identifier,bugprone-std-namespace-modification,cert-dcl37-c,cert-dcl51-cpp,cert-dcl58-cpp,readability-identifier-naming)
namespace std {

__attribute__((__noreturn__)) inline void __throw_logic_error(const char *) { __builtin_abort(); }
__attribute__((__noreturn__)) inline void __throw_length_error(const char *) { __builtin_abort(); }
__attribute__((__noreturn__)) inline void __throw_out_of_range(const char *) { __builtin_abort(); }
__attribute__((__noreturn__)) inline void __throw_out_of_range_fmt(const char *, ...) { __builtin_abort(); }

}  // namespace std
// NOLINTEND(bugprone-reserved-identifier,bugprone-std-namespace-modification,cert-dcl37-c,cert-dcl51-cpp,cert-dcl58-cpp,readability-identifier-naming)

#endif  // __cplusplus
