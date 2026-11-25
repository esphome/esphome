#pragma once
// Runtime service control for the retest: 1 create + queue start, 2 stop, 3 start, 4 remove (0x181A);
// 5 creates service 0x1820 + n (one 0x2B10 + n characteristic reading n 77) and logs the services vector (#22877).
#include "esphome/components/esp32_ble_server/ble_server.h"
namespace esphome::esp32_ble_server {
struct SvcInfo {
  size_t size;
  size_t cap;
  const void *data;
};
// reads the protected services_ through the explicit instantiation access rule
SvcInfo svc_info(const BLEServer &s);
template<auto M> struct SvcRob {
  friend SvcInfo svc_info(const BLEServer &s) { return {(s.*M).size(), (s.*M).capacity(), (s.*M).data()}; }
};
template struct SvcRob<&BLEServer::services_>;
}  // namespace esphome::esp32_ble_server
inline uint8_t rt_count = 0;
inline void rt_grow(bool in_cb) {
  using namespace esphome;
  auto *srv = esp32_ble_server::global_ble_server;
  auto before = esp32_ble_server::svc_info(*srv);
  uint8_t n = ++rt_count;
  auto *s = srv->create_service(esp32_ble::ESPBTUUID::from_uint16(0x1820 + n), false, 6);
  auto after = esp32_ble_server::svc_info(*srv);
  ESP_LOGI("T", "GROW %u %s size %u->%u cap %u->%u data %p->%p%s", n, in_cb ? "in_on_write" : "from_interval",
           (unsigned) before.size, (unsigned) after.size, (unsigned) before.cap, (unsigned) after.cap, before.data,
           after.data, before.data != after.data ? " MOVED" : "");
  if (s == nullptr)
    return;
  auto *c =
      s->create_characteristic(0x2B10 + n, esp32_ble_server::BLECharacteristic::PROPERTY_READ, ESP_GATT_PERM_READ);
  c->set_value({n, 0x77});
  srv->enqueue_start_service(s);
}
inline void rt_run(uint8_t cmd, bool in_cb) {
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
    case 5:
      rt_grow(in_cb);
      break;
    default:
      ESP_LOGW("T", "RT unknown command %u", cmd);
  }
}
