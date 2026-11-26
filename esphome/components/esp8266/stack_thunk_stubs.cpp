/*
 * Linker wrap stub for the Arduino core's BearSSL stack-overflow handler.
 *
 * stack_thunk_fatal_smashing() prints "FATAL ERROR: BSSL stack smashing detected" and then calls
 * __stack_chk_fail(). StackThunk is linked into every ESP8266 build, and on ESP8266 .rodata lives in
 * DRAM, so the message costs 44 bytes of RAM whether or not anything uses BearSSL. The postmortem
 * that __stack_chk_fail() prints already reports the stack smash.
 */

#if defined(USE_ESP8266)

namespace esphome::esp8266 {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

void __stack_chk_fail();
void __wrap_stack_thunk_fatal_smashing() { __stack_chk_fail(); }

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP8266
