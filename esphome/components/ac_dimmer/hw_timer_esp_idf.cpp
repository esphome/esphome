#ifdef USE_ESP32

#include "hw_timer_esp_idf.h"

#include "freertos/FreeRTOS.h"
#include "esphome/core/log.h"

#include "driver/gptimer.h"
#include "esp_clk_tree.h"
#include "soc/clk_tree_defs.h"

static const char *const TAG = "hw_timer_esp_idf";

namespace esphome::ac_dimmer {

// GPTimer divider constraints from ESP-IDF documentation
static constexpr uint32_t GPTIMER_DIVIDER_MIN = 2;
static constexpr uint32_t GPTIMER_DIVIDER_MAX = 65536;

using voidFuncPtr = void (*)();
using voidFuncPtrArg = void (*)(void *);

struct InterruptConfigT {
  voidFuncPtr fn{nullptr};
  void *arg{nullptr};
};

struct HWTimer {
  gptimer_handle_t timer_handle{nullptr};
  InterruptConfigT interrupt_handle{};
  bool timer_started{false};
};

HWTimer *timer_begin(uint32_t frequency) {
  esp_err_t err = ESP_OK;
  uint32_t counter_src_hz = 0;
  uint32_t divider = 0;
  soc_module_clk_t clk;
  for (auto clk_candidate : SOC_GPTIMER_CLKS) {
    clk = clk_candidate;
    esp_clk_tree_src_get_freq_hz(clk, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &counter_src_hz);
    divider = counter_src_hz / frequency;
    if ((divider >= GPTIMER_DIVIDER_MIN) && (divider <= GPTIMER_DIVIDER_MAX)) {
      break;
    } else {
      divider = 0;
    }
  }

  if (divider == 0) {
    ESP_LOGE(TAG, "Resolution not possible; aborting");
    return nullptr;
  }

  gptimer_config_t config = {
      .clk_src = static_cast<gptimer_clock_source_t>(clk),
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = frequency,
      .flags = {.intr_shared = true},
  };

  HWTimer *timer = new HWTimer();

  err = gptimer_new_timer(&config, &timer->timer_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPTimer creation failed; error %d", err);
    delete timer;
    return nullptr;
  }

  err = gptimer_enable(timer->timer_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPTimer enable failed; error %d", err);
    gptimer_del_timer(timer->timer_handle);
    delete timer;
    return nullptr;
  }

  err = gptimer_start(timer->timer_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPTimer start failed; error %d", err);
    gptimer_disable(timer->timer_handle);
    gptimer_del_timer(timer->timer_handle);
    delete timer;
    return nullptr;
  }

  timer->timer_started = true;
  return timer;
}

bool IRAM_ATTR timer_fn_wrapper(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *args) {
  auto *isr = static_cast<InterruptConfigT *>(args);
  if (isr->fn) {
    if (isr->arg) {
      reinterpret_cast<voidFuncPtrArg>(isr->fn)(isr->arg);
    } else {
      isr->fn();
    }
  }
  // Return false to indicate that no higher-priority task was woken and no context switch is requested.
  return false;
}

static void timer_attach_interrupt_functional_arg(HWTimer *timer, void (*user_func)(void *), void *arg) {
  if (timer == nullptr) {
    ESP_LOGE(TAG, "Timer handle is nullptr");
    return;
  }
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_fn_wrapper,
  };

  timer->interrupt_handle.fn = reinterpret_cast<voidFuncPtr>(user_func);
  timer->interrupt_handle.arg = arg;

  if (timer->timer_started) {
    gptimer_stop(timer->timer_handle);
  }
  gptimer_disable(timer->timer_handle);
  esp_err_t err = gptimer_register_event_callbacks(timer->timer_handle, &cbs, &timer->interrupt_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Timer Attach Interrupt failed; error %d", err);
  }
  gptimer_enable(timer->timer_handle);
  if (timer->timer_started) {
    gptimer_start(timer->timer_handle);
  }
}

void timer_attach_interrupt(HWTimer *timer, voidFuncPtr user_func) {
  timer_attach_interrupt_functional_arg(timer, reinterpret_cast<voidFuncPtrArg>(user_func), nullptr);
}

void timer_alarm(HWTimer *timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count) {
  if (timer == nullptr) {
    ESP_LOGE(TAG, "Timer handle is nullptr");
    return;
  }
  gptimer_alarm_config_t alarm_cfg = {
      .alarm_count = alarm_value,
      .reload_count = reload_count,
      .flags = {.auto_reload_on_alarm = autoreload},
  };
  esp_err_t err = gptimer_set_alarm_action(timer->timer_handle, &alarm_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Timer Alarm Write failed; error %d", err);
  }
}

}  // namespace esphome::ac_dimmer
#endif
