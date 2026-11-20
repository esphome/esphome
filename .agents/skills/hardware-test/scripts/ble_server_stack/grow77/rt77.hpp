#pragma once
// #22877 test: create a new service (0x1820 + n, one 0x2B10 + n characteristic reading n 77) and log the services
// vector.
#include "esphome/components/esp32_ble_server/ble_server.h"
namespace esphome::esp32_ble_server {
struct SvcInfo {
  size_t size;
  size_t cap;
  const void *data;
};
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
