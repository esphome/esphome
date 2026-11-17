#include "esphome/core/defines.h"
#ifdef USE_ESP32_PM_TICK_OVERFLOW_GUARD

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::esp32_power_management {}

// IDF drops tickless idle on chips without light sleep support (ESP32-H4 and -H21 on 5.5), and then
// vApplicationSleep() does not exist
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE

// Backport of IDF 6.0.1 CONFIG_PM_LIGHTSLEEP_TICK_OVERFLOW_PROTECTION: clamp a small tick overshoot
// after light sleep so vTaskStepTick() does not assert.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming,cppcoreguidelines-avoid-non-const-global-variables)
extern "C" {

void __real_vApplicationSleep(TickType_t expected_idle_time);
void __real_vTaskStepTick(TickType_t ticks_to_jump);

// Same default as IDF's CONFIG_PM_LIGHTSLEEP_TICK_OVERFLOW_TOLERANCE; a larger overshoot still asserts
static constexpr TickType_t TICK_OVERFLOW_TOLERANCE = 2;
static TickType_t expected_idle_time[portNUM_PROCESSORS];

void __wrap_vApplicationSleep(TickType_t expected) {
  expected_idle_time[xPortGetCoreID()] = expected;
  __real_vApplicationSleep(expected);
}

// Only pm_impl.c calls vTaskStepTick(), from inside vApplicationSleep() on the same core
void __wrap_vTaskStepTick(TickType_t ticks) {
  const TickType_t expected = expected_idle_time[xPortGetCoreID()];
  if (ticks > expected && ticks <= expected + TICK_OVERFLOW_TOLERANCE)
    ticks = expected;
  __real_vTaskStepTick(ticks);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming,cppcoreguidelines-avoid-non-const-global-variables)

#endif  // CONFIG_FREERTOS_USE_TICKLESS_IDLE
#endif  // USE_ESP32_PM_TICK_OVERFLOW_GUARD
