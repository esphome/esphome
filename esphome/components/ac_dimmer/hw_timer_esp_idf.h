#pragma once
#ifdef USE_ESP32_FRAMEWORK_ESP_IDF
#include "driver/timer.h"
namespace esphome {
namespace ac_dimmer {

typedef struct hw_timer_s {
  timer_group_t group;
  timer_idx_t num;
} hw_timer_t;

hw_timer_t *timerBegin(uint8_t timer, uint16_t divider, bool countUp);
void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);
void timerAlarmEnable(hw_timer_t *timer);
void timerAlarmDisable(hw_timer_t *timer);
void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload);
void timerSetAutoReload(hw_timer_t *timer, bool autoreload);
void timerStart(hw_timer_t *timer);
void timerStop(hw_timer_t *timer);

uint32_t timerGetConfig(hw_timer_t *timer);
uint16_t timerGetDivider(hw_timer_t *timer);
void timerSetDivider(hw_timer_t *timer, uint16_t divider);

}  // namespace ac_dimmer
}  // namespace esphome

#endif