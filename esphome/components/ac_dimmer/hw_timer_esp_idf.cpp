#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "hw_timer_esp_idf.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp32/clk.h"

#include "esphome/core/log.h"

static const char *const TAG = "hw_timer_esp_idf";

namespace esphome {
namespace ac_dimmer {

#define NUM_OF_TIMERS SOC_TIMER_GROUP_TOTAL_TIMERS

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

// Works for all chips
static hw_timer_t timer_dev[4] = {
    {TIMER_GROUP_0, TIMER_0}, {TIMER_GROUP_1, TIMER_0}, {TIMER_GROUP_0, TIMER_1}, {TIMER_GROUP_1, TIMER_1}};
// NOTE: (in IDF 5.0 there wont be need to know groups/numbers
// timer_init() will list thru all timers and return free timer handle)

typedef enum { APB_BEFORE_CHANGE, APB_AFTER_CHANGE } apb_change_ev_t;
typedef void (*apb_change_cb_t)(void *arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);
typedef struct apb_change_cb_s {
  struct apb_change_cb_s *prev;
  struct apb_change_cb_s *next;
  void *arg;
  apb_change_cb_t cb;
} apb_change_t;

static apb_change_t *apb_change_callbacks = NULL;
static xSemaphoreHandle apb_change_lock = NULL;

// Hz
uint32_t getApbFrequency() { return esp_clk_apb_freq(); }

static void _on_apb_change(void *arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb) {
  hw_timer_t *timer = (hw_timer_t *) arg;
  if (ev_type == APB_BEFORE_CHANGE) {
    timerStop(timer);
  } else {
    old_apb /= 1000000;
    new_apb /= 1000000;
    uint16_t divider = (new_apb * timerGetDivider(timer)) / old_apb;
    timerSetDivider(timer, divider);
    timerStart(timer);
  }
}

static void initApbChangeCallback() {
  static volatile bool initialized = false;
  if (!initialized) {
    initialized = true;
    apb_change_lock = xSemaphoreCreateMutex();
    if (!apb_change_lock) {
      initialized = false;
    }
  }
};

static bool addApbChangeCallback(void *arg, apb_change_cb_t cb) {
  initApbChangeCallback();
  apb_change_t *c = (apb_change_t *) malloc(sizeof(apb_change_t));
  if (!c) {
    ESP_LOGE(TAG, "Callback Object Malloc Failed");
    return false;
  }
  c->next = NULL;
  c->prev = NULL;
  c->arg = arg;
  c->cb = cb;
  xSemaphoreTake(apb_change_lock, portMAX_DELAY);
  if (apb_change_callbacks == NULL) {
    apb_change_callbacks = c;
  } else {
    apb_change_t *r = apb_change_callbacks;
    // look for duplicate callbacks
    while ((r != NULL) && !((r->cb == cb) && (r->arg == arg)))
      r = r->next;
    if (r) {
      ESP_LOGE(TAG, "duplicate func=%8p arg=%8p", c->cb, c->arg);
      free(c);
      xSemaphoreGive(apb_change_lock);
      return false;
    } else {
      c->next = apb_change_callbacks;
      apb_change_callbacks->prev = c;
      apb_change_callbacks = c;
    }
  }
  xSemaphoreGive(apb_change_lock);
  return true;
}

hw_timer_t *timerBegin(uint8_t num, uint16_t divider, bool countUp) {
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
  timerStart(timer);
  addApbChangeCallback(timer, _on_apb_change);
  return timer;
}

bool IRAM_ATTR timerFnWrapper(void *arg) {
  void (*fn)(void) = (void (*)()) arg;
  fn();

  // some additional logic or handling may be required here to approriately yield or not
  return false;
}

void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge) {
  if (edge) {
    ESP_LOGW(TAG, "EDGE timer interrupt is not supported! Setting to LEVEL...");
  }
  timer_isr_callback_add(timer->group, timer->num, (bool (*)(void *)) timerFnWrapper, (void *) fn, 0);
}

void timerStart(hw_timer_t *timer) { timer_start(timer->group, timer->num); }

void timerStop(hw_timer_t *timer) { timer_pause(timer->group, timer->num); }

void timerAlarmEnable(hw_timer_t *timer) { timer_set_alarm(timer->group, timer->num, TIMER_ALARM_EN); }

void timerAlarmDisable(hw_timer_t *timer) { timer_set_alarm(timer->group, timer->num, TIMER_ALARM_DIS); }

void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload) {
  timer_set_alarm_value(timer->group, timer->num, alarm_value);
  timerSetAutoReload(timer, autoreload);
}

void timerSetAutoReload(hw_timer_t *timer, bool autoreload) {
  timer_set_auto_reload(timer->group, timer->num, (timer_autoreload_t) autoreload);
}

uint16_t timerGetDivider(hw_timer_t *timer) {
  timer_cfg_t config;
  config.val = timerGetConfig(timer);
  return config.divider;
}

void timerSetDivider(hw_timer_t *timer, uint16_t divider) {
  if (divider < 2) {
    ESP_LOGE(TAG, "Timer divider must be set in range of 2 to 65535");
    return;
  }
  timer_set_divider(timer->group, timer->num, divider);
}

uint32_t timerGetConfig(hw_timer_t *timer) {
  timer_config_t timer_cfg;
  timer_get_config(timer->group, timer->num, &timer_cfg);

  // Translate to default uint32_t
  timer_cfg_t cfg;
  cfg.alarm_en = timer_cfg.alarm_en;
  cfg.autoreload = timer_cfg.auto_reload;
  cfg.divider = timer_cfg.divider;
  cfg.edge_int_en = timer_cfg.intr_type;
  cfg.level_int_en = !timer_cfg.intr_type;
  cfg.enable = timer_cfg.counter_en;
  cfg.increase = timer_cfg.counter_dir;

  return cfg.val;
}

}  // namespace ac_dimmer
}  // namespace esphome

#endif