#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <queue>
#include <mutex>
#include <cstring>

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * BLE events come in from a separate Task (thread) in the ESP32 stack. Rather
 * than trying to deal with various locking strategies, all incoming GAP and GATT
 * events will simply be placed on a semaphore guarded queue. The next time the
 * component runs loop(), these events are popped off the queue and handed at
 * this safer time.
 */

namespace esphome {
namespace esp32_ble {

template<class T> class Queue {
 public:
  Queue() { m_ = xSemaphoreCreateMutex(); }

  void push(T *element) {
    if (element == nullptr)
      return;
    if (xSemaphoreTake(m_, 5L / portTICK_PERIOD_MS)) {
      q_.push(element);
      xSemaphoreGive(m_);
    }
  }

  T *pop() {
    T *element = nullptr;

    if (xSemaphoreTake(m_, 5L / portTICK_PERIOD_MS)) {
      if (!q_.empty()) {
        element = q_.front();
        q_.pop();
      }
      xSemaphoreGive(m_);
    }
    return element;
  }

 protected:
  std::queue<T *> q_;
  SemaphoreHandle_t m_;
};

// Received GAP, GATTC and GATTS events are only queued, and get processed in the main loop().
// This class stores each event in a single type.
class BLEEvent {
 public:
  BLEEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    this->event_.gap.gap_event = e;
    memcpy(&this->event_.gap.gap_param, p, sizeof(esp_ble_gap_cb_param_t));
    this->type_ = GAP;
  };

  BLEEvent(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
    this->event_.gattc.gattc_event = e;
    this->event_.gattc.gattc_if = i;
    memcpy(&this->event_.gattc.gattc_param, p, sizeof(esp_ble_gattc_cb_param_t));
    // Need to also make a copy of notify event data.
    switch (e) {
      case ESP_GATTC_NOTIFY_EVT:
        memcpy(this->event_.gattc.data, p->notify.value, p->notify.value_len);
        this->event_.gattc.gattc_param.notify.value = this->event_.gattc.data;
        break;
      case ESP_GATTC_READ_CHAR_EVT:
      case ESP_GATTC_READ_DESCR_EVT:
        memcpy(this->event_.gattc.data, p->read.value, p->read.value_len);
        this->event_.gattc.gattc_param.read.value = this->event_.gattc.data;
        break;
      default:
        break;
    }
    this->type_ = GATTC;
  };

  BLEEvent(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
    this->event_.gatts.gatts_event = e;
    this->event_.gatts.gatts_if = i;
    memcpy(&this->event_.gatts.gatts_param, p, sizeof(esp_ble_gatts_cb_param_t));
    // Need to also make a copy of write data.
    switch (e) {
      case ESP_GATTS_WRITE_EVT:
        memcpy(this->event_.gatts.data, p->write.value, p->write.len);
        this->event_.gatts.gatts_param.write.value = this->event_.gatts.data;
        break;
      default:
        break;
    }
    this->type_ = GATTS;
  };

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gap_event {
      esp_gap_ble_cb_event_t gap_event;
      esp_ble_gap_cb_param_t gap_param;
    } gap;

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gattc_event {
      esp_gattc_cb_event_t gattc_event;
      esp_gatt_if_t gattc_if;
      esp_ble_gattc_cb_param_t gattc_param;
      uint8_t data[64];
    } gattc;

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gatts_event {
      esp_gatts_cb_event_t gatts_event;
      esp_gatt_if_t gatts_if;
      esp_ble_gatts_cb_param_t gatts_param;
      uint8_t data[64];
    } gatts;
  } event_;
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum ble_event_t : uint8_t {
    GAP,
    GATTC,
    GATTS,
  } type_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
