#pragma once
// One pulse shape per 35 s stage on GEN_PIN_A and GEN_PIN_B (BK72xx analogWrite) or GEN_PIN_NAME (AmebaZ2 pwmout)
#include <Arduino.h>
#ifdef GEN_PIN_NAME
// mbed pwmout API (its header clashes with Arduino's PinMode); pwmout_t is held in an oversized buffer
extern "C" {
void pwmout_init(void *obj, int pin);
void pwmout_period_us(void *obj, int us);
void pwmout_pulsewidth_us(void *obj, int us);
}
#endif
#include "esphome/core/log.h"
namespace pulsegen {
struct Stage {
  uint32_t period_us;
  uint32_t high_us;
};
static constexpr Stage STAGES[] = {
    {1000, 38},     {10000, 38},     {10000, 50},  {10000, 75},  {10000, 100},  {10000, 150},
    {10000, 200},   {1000, 200},     {10000, 300}, {10000, 500}, {10000, 1000}, {100000, 1000},
    {100000, 5000}, {100000, 50000}, {2000, 1000}, {1000, 500},  {250, 125},    {100000, 38},
};
static constexpr int N = sizeof(STAGES) / sizeof(STAGES[0]);
static constexpr uint32_t STAGE_MS = 35000;
#ifdef GEN_PIN_NAME
// Arduino analogWrite re-init faults on the 2nd period change on AmebaZ2: keep one pwmout and retune it
inline void apply(const Stage &st) {
  alignas(8) static uint8_t pwm[512];
  static bool init = false;
  if (!init) {
    pwmout_init(pwm, GEN_PIN_NAME);
    init = true;
  }
  pwmout_period_us(pwm, st.period_us);
  pwmout_pulsewidth_us(pwm, st.high_us);
}
#else
inline void apply_pin(uint8_t pin, const Stage &s) {
  pinMode(pin, INPUT);  // drop PWM mode so the new period is taken
  analogWriteResolution(16);
  analogWritePeriod(s.period_us);
  analogWrite(pin, (int) (65535.0f * s.high_us / s.period_us + 0.5f));
}
inline void apply(const Stage &st) {
  apply_pin(GEN_PIN_A, st);
  apply_pin(GEN_PIN_B, st);
}
#endif
inline void tick() {
  static int last = -1;
  int s = (millis() / STAGE_MS) % N;
  if (s == last)
    return;
  last = s;
  apply(STAGES[s]);
  ESP_LOGI("gen", "STAGE %d period %u high %u expect_per_min %.1f", s, STAGES[s].period_us, STAGES[s].high_us,
           60e6f / STAGES[s].period_us);
}
}  // namespace pulsegen
