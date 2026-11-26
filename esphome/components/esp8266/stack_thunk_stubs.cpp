/*
 * Linker wrap stub for the Arduino core's BearSSL stack-overflow handler.
 *
 * stack_thunk_fatal_smashing() prints "FATAL ERROR: BSSL stack smashing detected" and then calls
 * __stack_chk_fail(). Any build that links BearSSL (http_request HTTPS, the nextion upload, MQTT TLS)
 * links StackThunk, and on ESP8266 .rodata lives in DRAM, so the message costs 44 bytes of RAM. The
 * postmortem that __stack_chk_fail() prints already reports the stack smash. Builds without BearSSL
 * never reference the handler, so the wrap costs them nothing.
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
