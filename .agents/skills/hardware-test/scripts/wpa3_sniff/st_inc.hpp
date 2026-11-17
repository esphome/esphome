#pragma once
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <atomic>
static std::atomic<int64_t> s_conn_us{0};
static void st_register() {
  esp_err_t e = esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
      [](void *, esp_event_base_t, int32_t, void *) { s_conn_us.store(esp_timer_get_time()); }, nullptr);
  ESP_LOGI("st", "handler register: %d", e);
}
