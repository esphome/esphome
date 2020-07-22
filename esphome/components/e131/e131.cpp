#include "e131.h"
#include "e131_addressable_light_effect.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

namespace esphome {
namespace e131 {

static const char *TAG = "e131";
static const int PORT = 5568;

E131Component::E131Component() {}

E131Component::~E131Component() {
  if (udp_) {
    udp_->stop();
  }
}

void E131Component::setup() {
  udp_.reset(new WiFiUDP());

  if (!udp_->begin(PORT)) {
    ESP_LOGE(TAG, "Cannot bind E131 to %d.", PORT);
    mark_failed();
  }

  join_igmp_groups_();
}

void E131Component::loop() {
  std::vector<uint8_t> payload;
  E131Packet packet;
  int universe = 0;

  while (uint16_t packet_size = udp_->parsePacket()) {
    payload.resize(packet_size);

    if (!udp_->read(&payload[0], payload.size())) {
      continue;
    }

    if (!packet_(payload, universe, packet)) {
      ESP_LOGV(TAG, "Invalid packet recevied of size %zu.", payload.size());
      continue;
    }

    if (!process_(universe, packet)) {
      ESP_LOGV(TAG, "Ignored packet for %d universe of size %d.", universe, packet.count);
    }
  }
}

void E131Component::add_effect(E131AddressableLightEffect *light_effect) {
  if (light_effects_.count(light_effect)) {
    return;
  }

  ESP_LOGD(TAG, "Registering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.insert(light_effect);

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    join_(universe);
  }
}

void E131Component::remove_effect(E131AddressableLightEffect *light_effect) {
  if (!light_effects_.count(light_effect)) {
    return;
  }

  ESP_LOGD(TAG, "Unregistering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.erase(light_effect);

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    leave_(universe);
  }
}

bool E131Component::process_(int universe, const E131Packet &packet) {
  bool handled = false;

  ESP_LOGV(TAG, "Received E1.31 packet for %d universe, with %d bytes", universe, packet.count);

  for (auto light_effect : light_effects_) {
    handled = light_effect->process_(universe, packet) || handled;
  }

  return handled;
}

}  // namespace e131
}  // namespace esphome
