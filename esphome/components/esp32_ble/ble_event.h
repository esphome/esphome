#pragma once

#ifdef USE_ESP32

#include <vector>

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {
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
    // Need to also make a copy of relevant event data.
    switch (e) {
      case ESP_GATTC_NOTIFY_EVT:
        this->data.assign(p->notify.value, p->notify.value + p->notify.value_len);
        this->event_.gattc.gattc_param.notify.value = this->data.data();
        break;
      case ESP_GATTC_READ_CHAR_EVT:
      case ESP_GATTC_READ_DESCR_EVT:
        this->data.assign(p->read.value, p->read.value + p->read.value_len);
        this->event_.gattc.gattc_param.read.value = this->data.data();
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
    // Need to also make a copy of relevant event data.
    switch (e) {
      case ESP_GATTS_WRITE_EVT:
        this->data.assign(p->write.value, p->write.value + p->write.len);
        this->event_.gatts.gatts_param.write.value = this->data.data();
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
    } gattc;

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct gatts_event {
      esp_gatts_cb_event_t gatts_event;
      esp_gatt_if_t gatts_if;
      esp_ble_gatts_cb_param_t gatts_param;
    } gatts;
  } event_;

  std::vector<uint8_t> data{};
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
