#include "e131.h"
#include "e131_addressable_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace e131 {

static const char *TAG = "e131";
static const int MAX_UNIVERSES = 512;

void E131Component::add_effect(E131AddressableLightEffect *light_effect) {
  ESP_LOGD(TAG, "Registering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.insert(light_effect);
  should_rebind_ = true;
}

void E131Component::remove_effect(E131AddressableLightEffect *light_effect) {
  ESP_LOGD(TAG, "Unregistering '%s' for universes %d-%d.", light_effect->get_name().c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.erase(light_effect);
  should_rebind_ = true;
}

void E131Component::loop() {
  if (should_rebind_) {
    should_rebind_ = false;
    rebind_();
  }

  if (!e131_client_) {
    return;
  }

  while (!e131_client_->isEmpty()) {
    e131_packet_t packet;
    e131_client_->pull(&packet);  // Pull packet from ring buffer
    auto universe = htons(packet.universe);
    auto handled = process_(universe, packet);

    if (!handled) {
      ESP_LOGV(TAG, "Ignored packet for %d universe of size %d.", universe, packet.property_value_count);
    }
  }
}

void E131Component::rebind_() {
  e131_client_.reset();

  int first_universe = MAX_UNIVERSES;
  int last_universe = 0;
  int universe_count = 0;

  for (auto light_effect : light_effects_) {
    first_universe = std::min(first_universe, light_effect->get_first_universe());
    last_universe = std::max(last_universe, light_effect->get_last_universe());
    universe_count += light_effect->get_universe_count();
  }

  if (!universe_count) {
    ESP_LOGI(TAG, "Stopped E1.31.");
    return;
  }

  e131_client_.reset(new ESPAsyncE131(universe_count));

  bool success = e131_client_->begin(listen_method_, first_universe, last_universe - first_universe + 1);

  if (success) {
    ESP_LOGI(TAG, "Started E1.31 for universes %d-%d", first_universe, last_universe);
  } else {
    ESP_LOGW(TAG, "Failed to start E1.31 for universes %d-%d", first_universe, last_universe);
  }
}

bool E131Component::process_(int universe, const e131_packet_t &packet) {
  bool handled = false;

  ESP_LOGV(TAG, "Received E1.31 packet for %d universe, with %d bytes", universe, packet.property_value_count);

  for (auto light_effect : light_effects_) {
    handled = light_effect->process_(universe, packet) || handled;
  }

  return handled;
}

}  // namespace e131
}  // namespace esphome
