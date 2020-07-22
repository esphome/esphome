#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <queue>
#include <mutex>

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

/*
 * BLE events come in from a separate Task (thread) in the ESP32 stack. Rather
 * than trying to deal wth various locking strategies, all incoming GAP and GATT
 * events will simply be placed on a semaphore guarded queue. The next time the
 * component runs loop(), these events are popped off the queue and handed at
 * this safer time.
 */

namespace esphome {
namespace esp32_ble_tracker {

template<class T> class Queue {
 public:
  Queue() { m = xSemaphoreCreateMutex(); }

  void push(T *element) {
    if (element == nullptr)
      return;
    if (xSemaphoreTake(m, 5L / portTICK_PERIOD_MS)) {
      q.push(element);
      xSemaphoreGive(m);
    }
  }

  T *pop() {
    T *element = nullptr;

    if (xSemaphoreTake(m, 5L / portTICK_PERIOD_MS)) {
      if (!q.empty()) {
        element = q.front();
        q.pop();
      }
      xSemaphoreGive(m);
    }
    return element;
  }

 protected:
  std::queue<T *> q;
  SemaphoreHandle_t m;
};

// Received GAP and GATTC events are only queued, and get processed in the main loop().
// This class stores each event in a single type.
class BLEEvent {
 public:
  BLEEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    gap_event = e;
    memcpy(&gap_param, p, sizeof(esp_ble_gap_cb_param_t));
    type_ = 0;
  };

  BLEEvent(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    gattc_event = e;
    gattc_if = i;
    memcpy(&gattc_param, p, sizeof(esp_ble_gattc_cb_param_t));
    type_ = 1;
  };

  esp_gap_ble_cb_event_t gap_event;
  esp_ble_gap_cb_param_t gap_param;

  esp_gattc_cb_event_t gattc_event;
  esp_gatt_if_t gattc_if;
  esp_ble_gattc_cb_param_t gattc_param;

  uint8_t type_;  // 0=gap 1=gattc
};

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
