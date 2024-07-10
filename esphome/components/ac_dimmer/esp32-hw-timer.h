#pragma once
#include "driver/timer.h"
namespace esphome {
namespace ac_dimmer {

typedef struct hw_timer_s {
  timer_group_t group;
  timer_idx_t num;
} hw_timer_t;

class EspIdfTimer {
 public:
  static hw_timer_t *timerBegin(uint8_t timer, uint16_t divider, bool countUp);
  static void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);

  static void timerAlarmEnable(hw_timer_t *timer);
  static void timerAlarmDisable(hw_timer_t *timer);
  static void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload);
  static void timerSetAutoReload(hw_timer_t *timer, bool autoreload);

  static void timerStart(hw_timer_t *timer);
};
}  // namespace ac_dimmer
}  // namespace esphome