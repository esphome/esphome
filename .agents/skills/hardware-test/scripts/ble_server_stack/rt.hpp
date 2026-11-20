#pragma once
// Runtime service control for the retest: 1 create + queue start, 2 stop, 3 start, 4 remove (0x181A).
#include "esphome/components/esp32_ble_server/ble_server.h"
inline void rt_run(uint8_t cmd) {
  using namespace esphome;
  auto *srv = esp32_ble_server::global_ble_server;
  auto uuid = esp32_ble::ESPBTUUID::from_uint16(0x181A);
  auto *s = srv->get_service(uuid);
  switch (cmd) {
    case 1: {
      s = srv->create_service(uuid, false, 10);
      if (s == nullptr) {
        ESP_LOGW("T", "RT create returned null");
        break;
      }
      auto *c =
          s->create_characteristic(0x2A6E, esp32_ble_server::BLECharacteristic::PROPERTY_READ, ESP_GATT_PERM_READ);
      c->set_value({0x34, 0x12});
      srv->enqueue_start_service(s);
      ESP_LOGI("T", "RT created");
      break;
    }
    case 2:
      if (s != nullptr)
        s->stop();
      ESP_LOGI("T", "RT stop %s", s != nullptr ? "ok" : "missing");
      break;
    case 3:
      if (s != nullptr)
        s->start();
      ESP_LOGI("T", "RT start %s", s != nullptr ? "ok" : "missing");
      break;
    case 4:
      srv->remove_service(uuid);
      ESP_LOGI("T", "RT removed, get_service now %s", srv->get_service(uuid) == nullptr ? "null" : "found");
      break;
    default:
      ESP_LOGW("T", "RT unknown command %u", cmd);
  }
}
