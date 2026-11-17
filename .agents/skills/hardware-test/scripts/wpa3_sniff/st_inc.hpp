#pragma once
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <atomic>
#include "esphome/core/application.h"
// STA_CONNECTED time from the event task; the handler wakes the main loop so on_loop sees it on the next pass.
static std::atomic<int64_t> s_conn_us{0};
static void st_register() {
  esp_err_t e = esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
      [](void *, esp_event_base_t, int32_t, void *) {
        s_conn_us.store(esp_timer_get_time());
        esphome::App.wake_loop_threadsafe();
      },
      nullptr);
  ESP_LOGI("st", "handler register: %d", e);
}
