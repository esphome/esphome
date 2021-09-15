#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

void setup();
void loop();

namespace esphome {

void IRAM_ATTR HOT yield() {
  vPortYield();
}
uint32_t IRAM_ATTR HOT millis() {
  return (uint32_t) (esp_timer_get_time() / 1000ULL);
}
void IRAM_ATTR HOT delay(uint32_t ms) {
  vTaskDelay(ms / portTICK_PERIOD_MS);
}
uint32_t IRAM_ATTR HOT micros() {
    return (uint32_t) esp_timer_get_time();
}
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  auto start = (uint64_t) esp_timer_get_time();
  while (((uint64_t) esp_timer_get_time()) - start < us);
}
void arch_restart() {
  esp_restart();
  // restart() doesn't always end execution
  while (true) {
    yield();
  }
}
void IRAM_ATTR HOT arch_feed_wdt() {
  // TODO: copy comment from app_esp32.cpp
  delay(1);
}

TaskHandle_t loop_task_handle = nullptr;

void loop_task(void *pvParameters) {
  setup();
  while (true) {
    loop();
  }
}

extern "C" void app_main() {
  esp32::setup_preferences();
  xTaskCreate(
    loop_task,
    "loopTask",
    8192,
    nullptr,
    1,
    &loop_task_handle
  );
}

}  // namespace esphome
