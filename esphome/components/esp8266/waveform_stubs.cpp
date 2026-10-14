#ifdef USE_ESP8266_WAVEFORM_STUBS

// Stub implementations for Arduino waveform/PWM functions.
//
// When the waveform generator is not needed (no esp8266_pwm component),
// we exclude core_esp8266_waveform_pwm.cpp from the build to save ~596 bytes
// of RAM and 464 bytes of flash.
//
// These stubs satisfy calls from the Arduino GPIO code when the real
// waveform implementation is excluded. They must be in the global namespace
// with C linkage to match the Arduino core function declarations.

#include <cstdint>

// Empty namespace to satisfy linter - actual stubs must be at global scope
namespace esphome::esp8266 {}  // namespace esphome::esp8266

extern "C" {

// Called by Arduino GPIO code to stop any waveform on a pin
int stopWaveform(uint8_t pin) {
  (void) pin;
  return 1;  // Success (no waveform to stop)
}

// Called by Arduino GPIO code to stop any PWM on a pin
bool _stopPWM(uint8_t pin) {
  (void) pin;
  return false;  // No PWM was running
}

}  // extern "C"

#endif  // USE_ESP8266_WAVEFORM_STUBS
