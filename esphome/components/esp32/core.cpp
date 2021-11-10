#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include <soc/rtc.h>
#include <soc/timer_group_struct.h>
#include <soc/timer_group_reg.h>

#if ESP_IDF_VERSION_MAJOR >= 4
#include <hal/cpu_hal.h>
#endif

void setup();
void loop();

namespace esphome {

void IRAM_ATTR HOT yield() { vPortYield(); }
uint32_t IRAM_ATTR HOT millis() { return (uint32_t)(esp_timer_get_time() / 1000ULL); }
void IRAM_ATTR HOT delay(uint32_t ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }
uint32_t IRAM_ATTR HOT micros() { return (uint32_t) esp_timer_get_time(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  esp_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void IRAM_ATTR HOT arch_feed_wdt() {
#ifdef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
  // ESP32 uses "Task Watchdog" which is hooked to the FreeRTOS idle task.
  // Using esp_task_wdt_reset() would only feed the watchdog if the idle task
  // was executed recently. Another option is to yield here with vTaskDelay(1),
  // but the fast/consistent method is to reset the registers directly:

  TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;  // write enable
  TIMERG0.wdt_feed = 1;                        // feed dog 0
  TIMERG0.wdt_wprotect = 0;                    // write protect
#endif
}

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
uint32_t arch_get_cpu_cycle_count() {
#if ESP_IDF_VERSION_MAJOR >= 4
  return cpu_hal_get_cycle_count();
#else
  uint32_t ccount;
  __asm__ __volatile__("esync; rsr %0,ccount" : "=a"(ccount));
  return ccount;
#endif
}
uint32_t arch_get_cpu_freq_hz() { return rtc_clk_apb_freq_get(); }

#ifdef USE_ESP_IDF
TaskHandle_t loop_task_handle = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void loop_task(void *pv_params) {
  setup();
  while (true) {
    loop();
  }
}

extern "C" void app_main() {
  esp32::setup_preferences();
  esp_pm_config_esp32s2_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,
    .light_sleep_enable = true
    };
  esp_pm_configure(&pm_config);
  xTaskCreate(loop_task, "loopTask", 8192, nullptr, 1, &loop_task_handle);
}
#endif  // USE_ESP_IDF

#ifdef USE_ARDUINO
extern "C" void init() { esp32::setup_preferences(); }
#endif  // USE_ARDUINO

}  // namespace esphome

#endif  // USE_ESP32
