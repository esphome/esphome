#pragma once
// bw12isr: pull-toggle edge generator on floating pins, ISR read counters and pin/IRQ register dump (RTL8710B)
#include <Arduino.h>
#include "esphome/core/log.h"
extern "C" {
PinStatus __real_digitalRead(pin_size_t pin);
__attribute__((long_call)) void PAD_PullCtrl(uint8_t pin, uint8_t pull);
}
namespace bwdbg {
static constexpr uint8_t PINS[] = {12, 0, 14, 15, 19, 22};
static constexpr int NP = sizeof(PINS);
inline volatile uint32_t isr_reads[32], task_reads[32];
inline uint32_t gen_rise = 0, gen_fall = 0, checks = 0, mismatch[NP];
inline bool up = false, started = false;
inline uint32_t reg(uint32_t off) { return *reinterpret_cast<volatile uint32_t *>(0x40001000 + off); }
inline bool in_isr() {
  uint32_t r;
  __asm volatile("mrs %0, ipsr" : "=r"(r));
  return r != 0;
}
// Every call: check each pin follows the pull set on the previous call, then flip all pulls
inline void tick() {
  uint32_t lvl = reg(0x50);
  if (started) {
    checks++;
    for (int i = 0; i < NP; i++)
      if (((lvl >> PINS[i]) & 1) != (up ? 1u : 0u))
        mismatch[i]++;
  }
  up = !up;
  started = true;
  for (int i = 0; i < NP; i++)
    PAD_PullCtrl(PINS[i], up ? 2 : 1);  // GPIO_PuPd_UP : GPIO_PuPd_DOWN
  if (up)
    gen_rise++;
  else
    gen_fall++;
}
inline void report() {
  uint32_t en = reg(0x30), mask = reg(0x34), type = reg(0x38), pol = reg(0x3C), deb = reg(0x48), lvl = reg(0x50);
  ESP_LOGI("dbg", "gen rise %u fall %u checks %u", gen_rise, gen_fall, checks);
  for (int i = 0; i < NP; i++) {
    uint8_t p = PINS[i];
    PinInfo *info = pinInfo(p);
    // -1 marks a pin with no PinInfo entry
    const uint32_t e = info ? info->enabled : 0;
    const int gpio = info ? !!(e & PIN_GPIO) : -1, irq = info ? !!(e & PIN_IRQ) : -1, pwm = info ? !!(e & PIN_PWM) : -1;
    ESP_LOGI("dbg",
             "PA%02u LT 0x%x (GPIO %d IRQ %d PWM %d) HW int_en %u mask %u edge %u pol_high %u deb %u lvl %u | "
             "mismatch %u isr digitalRead %u task digitalRead %u",
             p, e, gpio, irq, pwm, (en >> p) & 1, (mask >> p) & 1, (type >> p) & 1, (pol >> p) & 1, (deb >> p) & 1,
             (lvl >> p) & 1, mismatch[i], isr_reads[p], task_reads[p]);
  }
}
}  // namespace bwdbg
extern "C" __attribute__((used)) PinStatus __wrap_digitalRead(pin_size_t pin) {
  if (pin < 32) {
    if (bwdbg::in_isr())
      bwdbg::isr_reads[pin] = bwdbg::isr_reads[pin] + 1;
    else
      bwdbg::task_reads[pin] = bwdbg::task_reads[pin] + 1;
  }
  return __real_digitalRead(pin);
}
