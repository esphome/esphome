#pragma once
#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "esp_idf_version.h"

#if (ESP_IDF_VERSION_MAJOR == 4)
#include "driver/timer.h"
#endif

#if (ESP_IDF_VERSION_MAJOR == 5)
#include "driver/gptimer_types.h"
#endif

namespace esphome {
namespace ac_dimmer {

#if (ESP_IDF_VERSION_MAJOR == 4)
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
#endif

#if (ESP_IDF_VERSION_MAJOR == 5)

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

struct interrupt_config_t {
  voidFuncPtr fn;
  void *arg;
};

struct hw_timer_t {
  gptimer_handle_t timer_handle;
  interrupt_config_t interrupt_handle;
  bool timer_started;
};

hw_timer_t *timerBegin(uint32_t frequency);

void timerAttachInterrupt(hw_timer_t *timer, void (*userFunc)(void));
void timerAlarm(hw_timer_t *timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);
void timerStart(hw_timer_t *timer);

#endif
}  // namespace ac_dimmer
}  // namespace esphome

#endif
