#pragma once
#ifdef USE_ESP32

#include "driver/gptimer_types.h"

namespace esphome::ac_dimmer {

struct HWTimer;

HWTimer *timer_begin(uint32_t frequency);

void timer_attach_interrupt(HWTimer *timer, void (*user_func)());
void timer_alarm(HWTimer *timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);

}  // namespace esphome::ac_dimmer

#endif
