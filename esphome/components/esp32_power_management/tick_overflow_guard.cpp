#include "esphome/core/defines.h"
#ifdef USE_ESP32_PM_TICK_OVERFLOW_GUARD

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::esp32_power_management {}

// Backport of IDF 6.0.1's CONFIG_PM_LIGHTSLEEP_TICK_OVERFLOW_PROTECTION. After an automatic light sleep,
// pm_impl.c steps the tick count by the time slept, which can exceed the expected idle time by a tick and
// trip the configASSERT in vTaskStepTick(). Clamp a small overshoot to the expected idle time instead.
// Names are mandated by the --wrap linker mechanism.
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

void __wrap_vTaskStepTick(TickType_t ticks) {
  const TickType_t expected = expected_idle_time[xPortGetCoreID()];
  if (ticks > expected && ticks <= expected + TICK_OVERFLOW_TOLERANCE)
    ticks = expected;
  __real_vTaskStepTick(ticks);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming,cppcoreguidelines-avoid-non-const-global-variables)

#endif  // USE_ESP32_PM_TICK_OVERFLOW_GUARD
