#ifdef USE_ESP8266_WAVEFORM_STUBS

// Stub implementations for Arduino waveform/PWM functions.
//
// When the waveform generator is not needed (no esp8266_pwm component),
// we exclude core_esp8266_waveform_pwm.cpp from the build to save ~580 bytes
// of RAM (wvfState 512B + pwmState 68B).
//
// However, digitalWrite() unconditionally calls stopWaveform() and _stopPWM()
// to ensure any active waveform is stopped before changing pin state.
// These stubs satisfy those calls when the real waveform code is excluded.

#include <cstdint>

extern "C" {

// Called by digitalWrite() to stop any waveform on a pin
int stopWaveform(uint8_t pin) {
  (void) pin;
  return 1;  // Success (no waveform to stop)
}

// Called by digitalWrite() to stop any PWM on a pin
bool _stopPWM(uint8_t pin) {
  (void) pin;
  return false;  // No PWM was running
}

}  // extern "C"

#endif  // USE_ESP8266_WAVEFORM_STUBS
