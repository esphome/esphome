#include "e131.h"
#include "e131_addressable_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace e131 {

static const char *const TAG = "e131";
static const int PORT = 5568;

E131Component::E131Component() {}

E131Component::~E131Component() {
  if (this->socket_) {
    this->socket_->close();
  }
}

void E131Component::setup() {
  this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);

  int enable = 1;
  int err = this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = this->socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), PORT);
  if (sl == 0) {
    ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = this->socket_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  join_igmp_groups_();
}

void E131Component::loop() {
  std::vector<uint8_t> payload;
  E131Packet packet;
  int universe = 0;
  uint8_t buf[1460];

  ssize_t len = this->socket_->read(buf, sizeof(buf));
  if (len == -1) {
    return;
  }
  payload.resize(len);
  memmove(&payload[0], buf, len);

  if (!this->packet_(payload, universe, packet)) {
    ESP_LOGV(TAG, "Invalid packet received of size %zu.", payload.size());
    return;
  }

  if (!this->process_(universe, packet)) {
    ESP_LOGV(TAG, "Ignored packet for %d universe of size %d.", universe, packet.count);
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

  for (auto *light_effect : light_effects_) {
    handled = light_effect->process_(universe, packet) || handled;
  }

  return handled;
}

}  // namespace e131
}  // namespace esphome
