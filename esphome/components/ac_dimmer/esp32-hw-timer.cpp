#include "esp32-hw-timer.h"
// #include "soc/soc_caps.h"
// #include "esp_clk.h"
// #include "esp32/clk.h"

#include "esphome/core/log.h"

static const char *const TAG = "esp32-hal-timer";

namespace esphome {
namespace ac_dimmer {

typedef union {
  struct {
    uint32_t reserved0 : 10;
    uint32_t alarm_en : 1;     /*When set  alarm is enabled*/
    uint32_t level_int_en : 1; /*When set  level type interrupt will be generated during alarm*/
    uint32_t edge_int_en : 1;  /*When set  edge type interrupt will be generated during alarm*/
    uint32_t divider : 16;     /*Timer clock (T0/1_clk) pre-scale value.*/
    uint32_t autoreload : 1;   /*When set  timer 0/1 auto-reload at alarming is enabled*/
    uint32_t increase : 1;     /*When set  timer 0/1 time-base counter increment. When cleared timer 0 time-base counter
                                  decrement.*/
    uint32_t enable : 1;       /*When set timer 0/1 time-base counter is enabled*/
  };
  uint32_t val;
} timer_cfg_t;

#define NUM_OF_TIMERS SOC_TIMER_GROUP_TOTAL_TIMERS

// Works for all chips
static hw_timer_t timer_dev[4] = {
    {TIMER_GROUP_0, TIMER_0}, {TIMER_GROUP_1, TIMER_0}, {TIMER_GROUP_0, TIMER_1}, {TIMER_GROUP_1, TIMER_1}};

// NOTE: (in IDF 5.0 there wont be need to know groups/numbers
// timer_init() will list thru all timers and return free timer handle)

// uint32_t getApbFrequency() {
//   // rtc_clk_apb_freq_get();
//   return esp_clk_apb_freq();
// }

// static void _on_apb_change(void *arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb) {
//   hw_timer_t *timer = (hw_timer_t *) arg;
//   if (ev_type == APB_BEFORE_CHANGE) {
//     timerStop(timer);
//   } else {
//     old_apb /= 1000000;
//     new_apb /= 1000000;
//     uint16_t divider = (new_apb * timerGetDivider(timer)) / old_apb;
//     timerSetDivider(timer, divider);
//     timerStart(timer);
//   }
// }

hw_timer_t *EspIdfTimer::timerBegin(uint8_t num, uint16_t divider, bool countUp) {
  if (num >= NUM_OF_TIMERS) {
    ESP_LOGE(TAG, "Timer number %u exceeds available number of Timers.", num);
    return NULL;
  }

  hw_timer_t *timer = &timer_dev[num];  // Get Timer group/num from 0-3 number

  timer_config_t config = {
      .alarm_en = TIMER_ALARM_DIS,
      .counter_en = TIMER_PAUSE,
      .intr_type = TIMER_INTR_LEVEL,
      .counter_dir = static_cast<timer_count_dir_t>(countUp),
      .auto_reload = timer_autoreload_t::TIMER_AUTORELOAD_DIS,
      .divider = divider,
  };

  timer_init(timer->group, timer->num, &config);
  timer_set_counter_value(timer->group, timer->num, 0);
  EspIdfTimer::timerStart(timer);
  //  addApbChangeCallback(timer, _on_apb_change);
  return timer;
}

bool IRAM_ATTR timerFnWrapper(void *arg) {
  void (*fn)(void) = (void (*)()) arg;
  fn();

  // some additional logic or handling may be required here to approriately yield or not
  return false;
}

void EspIdfTimer::timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge) {
  if (edge) {
    ESP_LOGW(TAG, "EDGE timer interrupt is not supported! Setting to LEVEL...");
  }
  timer_isr_callback_add(timer->group, timer->num, (bool (*)(void *)) timerFnWrapper, (void *) fn, 0);
}

void EspIdfTimer::timerStart(hw_timer_t *timer) { timer_start(timer->group, timer->num); }

void EspIdfTimer::timerAlarmEnable(hw_timer_t *timer) { timer_set_alarm(timer->group, timer->num, TIMER_ALARM_EN); }

void EspIdfTimer::timerAlarmDisable(hw_timer_t *timer) { timer_set_alarm(timer->group, timer->num, TIMER_ALARM_DIS); }

void EspIdfTimer::timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload) {
  timer_set_alarm_value(timer->group, timer->num, alarm_value);
  timerSetAutoReload(timer, autoreload);
}

void EspIdfTimer::timerSetAutoReload(hw_timer_t *timer, bool autoreload) {
  timer_set_auto_reload(timer->group, timer->num, (timer_autoreload_t) autoreload);
}

}  // namespace ac_dimmer
}  // namespace esphome