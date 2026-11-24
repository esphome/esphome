#pragma once
// Test only, LibreTiny (esphome#23104): records when AsyncMqttClient's disconnect callback runs, so on_disconnect can
// log the delay to the main loop.
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/core/hal.h"

namespace hwtlat {
template<typename Tag, typename Tag::type M> struct Rob {
  friend typename Tag::type get(Tag) { return M; }
};
struct BackendTag {
  using type = esphome::mqtt::MQTTBackendLibreTiny esphome::mqtt::MQTTClientComponent::*;
  friend type get(BackendTag);
};
template struct Rob<BackendTag, &esphome::mqtt::MQTTClientComponent::mqtt_backend_>;
struct ClientTag {
  using type = AsyncMqttClient esphome::mqtt::MQTTBackendLibreTiny::*;
  friend type get(ClientTag);
};
template struct Rob<ClientTag, &esphome::mqtt::MQTTBackendLibreTiny::mqtt_client_>;
}  // namespace hwtlat

static volatile uint32_t hwt_cb_us = 0;
static volatile uint32_t hwt_cb_n = 0;

inline void hwt_hook(esphome::mqtt::MQTTClientComponent *c) {
  auto &backend = c->*get(hwtlat::BackendTag{});
  AsyncMqttClient &client = backend.*get(hwtlat::ClientTag{});
  client.onDisconnect([](AsyncMqttClientDisconnectReason) {
    hwt_cb_us = micros();
    hwt_cb_n++;
  });
}
inline uint32_t hwt_lat_us() { return micros() - hwt_cb_us; }
