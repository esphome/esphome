#pragma once
// pulse22391b: drive GEN_PIN_A and GEN_PIN_B with the same pulse train, one shape per stage
#include <Arduino.h>
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
inline void apply(uint8_t pin, const Stage &s) {
  pinMode(pin, INPUT);  // drop PWM mode so the new period is taken
  analogWriteResolution(16);
  analogWritePeriod(s.period_us);
  analogWrite(pin, (int) (65535.0f * s.high_us / s.period_us + 0.5f));
}
inline void tick() {
  static int last = -1;
  int s = (millis() / STAGE_MS) % N;
  if (s == last)
    return;
  last = s;
  apply(GEN_PIN_A, STAGES[s]);
  apply(GEN_PIN_B, STAGES[s]);
  ESP_LOGI("gen", "STAGE %d period %u high %u expect_per_min %.1f", s, STAGES[s].period_us, STAGES[s].high_us,
           60e6f / STAGES[s].period_us);
}
}  // namespace pulsegen
