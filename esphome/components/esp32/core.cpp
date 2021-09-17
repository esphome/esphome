#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include <soc/rtc.h>

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
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  auto start = (uint64_t) esp_timer_get_time();
  while (((uint64_t) esp_timer_get_time()) - start < us)
    ;
}
void arch_restart() {
  esp_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void IRAM_ATTR HOT arch_feed_wdt() {
#ifdef USE_ARDUINO
#if CONFIG_ARDUINO_RUNNING_CORE == 0
#ifdef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
  // ESP32 uses "Task Watchdog" which is hooked to the FreeRTOS idle task.
  // To cause the Watchdog to be triggered we need to put the current task
  // to sleep to get the idle task scheduled.
  delay(1);
#endif
#endif
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
#ifdef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
  delay(1);
#endif
#endif  // USE_ESP_IDF
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
  xTaskCreate(loop_task, "loopTask", 8192, nullptr, 1, &loop_task_handle);
}
#endif  // USE_ESP_IDF

#ifdef USE_ARDUINO
extern "C" void init() { esp32::setup_preferences(); }
#endif  // USE_ARDUINO

}  // namespace esphome

#endif  // USE_ESP32
