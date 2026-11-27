#include "e131.h"
#ifdef USE_NETWORK
#include "e131_addressable_light_effect.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#ifdef USE_E131_ARBITRATION
#include "esphome/core/application.h"
#endif

#include <algorithm>
#ifdef USE_E131_ARBITRATION
#include <cstring>
#endif

namespace esphome::e131 {

ESPHOME_LOG_TAG(TAG, "e131");
static const int PORT = 5568;

E131Component::E131Component() {}

E131Component::~E131Component() {
  if (this->socket_) {
    this->socket_->close();
  }
}

void E131Component::setup() {
  this->socket_ = socket::socket_ip_udp(IPPROTO_IP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Could not create socket: errno %d", errno);
    this->mark_failed();
    return;
  }

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
  if (this->igmp_join_pending_ && network::is_connected())
    this->join_igmp_groups_();
  E131Packet packet;
  int universe = 0;
  uint8_t buf[1460];
  ssize_t len;

  // Drain all queued packets so multi-universe frames are applied
  // atomically before the light writes. Without this, each universe
  // packet would trigger a separate full-strip write causing tearing.
  while ((len = this->socket_->read(buf, sizeof(buf))) > 0) {
    if (!this->packet_(buf, (size_t) len, universe, packet)) {
      ESP_LOGV(TAG, "Invalid packet received of size %d.", (int) len);
      continue;
    }

#ifdef USE_E131_ARBITRATION
    E131SourceInfo source;
    this->source_info_(buf, source);
    const E131Decision decision = this->arbitrate_(universe, source);
    if (decision == E131Decision::E131_DECISION_IGNORE)
      continue;
    if (packet.count < E131_MAX_PROPERTY_VALUES_COUNT) {
      // Clear omitted slots for all consumers, including effects that joined after the source took over.
      memset(packet.values + packet.count, 0, E131_MAX_PROPERTY_VALUES_COUNT - packet.count);
      packet.count = E131_MAX_PROPERTY_VALUES_COUNT;
    }
#endif

    if (!this->process_(universe, packet)) {
      ESP_LOGV(TAG, "Ignored packet for %d universe of size %d.", universe, packet.count);
    }
  }
}

void E131Component::add_effect(E131AddressableLightEffect *light_effect) {
  if (std::find(light_effects_.begin(), light_effects_.end(), light_effect) != light_effects_.end()) {
    return;
  }

  auto effect_name = light_effect->get_name();
  ESP_LOGD(TAG, "Registering '%.*s' for universes %d-%d.", (int) effect_name.size(), effect_name.c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  light_effects_.push_back(light_effect);

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    join_(universe);
  }
}

void E131Component::remove_effect(E131AddressableLightEffect *light_effect) {
  auto it = std::find(light_effects_.begin(), light_effects_.end(), light_effect);
  if (it == light_effects_.end()) {
    return;
  }

  auto effect_name = light_effect->get_name();
  ESP_LOGD(TAG, "Unregistering '%.*s' for universes %d-%d.", (int) effect_name.size(), effect_name.c_str(),
           light_effect->get_first_universe(), light_effect->get_last_universe());

  // Swap with last element and pop for O(1) removal (order doesn't matter)
  *it = light_effects_.back();
  light_effects_.pop_back();

  for (auto universe = light_effect->get_first_universe(); universe <= light_effect->get_last_universe(); ++universe) {
    leave_(universe);
  }
}

#ifdef USE_E131_ARBITRATION
// Decides whether a valid data packet is the data of its universe (ANSI E1.31-2025, 6.2.3): one source per
// universe, the highest priority wins, and at equal priority the lowest CID, so that the same set of sources
// always gives the same result (6.2.3.3).
E131Decision E131Component::arbitrate_(int universe, const E131SourceInfo &info) {
  if (info.options & E131_OPTION_PREVIEW_DATA)
    return E131Decision::E131_DECISION_IGNORE;  // not for live output (6.2.6)
  if (info.priority > E131_MAX_PRIORITY)
    return E131Decision::E131_DECISION_IGNORE;  // never transmitted by a conforming source (6.2.3)

  auto *consumer = this->find_universe_(universe);
  if (consumer == nullptr || consumer->consumers == 0)
    return E131Decision::E131_DECISION_IGNORE;  // no effect listens to this universe

  // Logging may run automations that start effects and reallocate the universe entries, so the source state is
  // only used before the log calls at the end.
  E131Source &winner = consumer->source;
  const uint32_t now = App.get_loop_component_start_time();
  uint8_t left_cid[E131_CID_SIZE];
  const char *left_reason = nullptr;
  E131Decision decision = E131Decision::E131_DECISION_IGNORE;

  // Network data loss (6.7.1): the winner, or the runner-up, sent nothing for the timeout.
  if (winner.active && now - winner.last_packet_ms >= E131_NETWORK_DATA_LOSS_TIMEOUT_MS) {
    winner.active = false;
    memcpy(left_cid, winner.cid, E131_CID_SIZE);
    left_reason = "lost";
  }
  if (winner.has_runner_up && now - winner.runner_up_ms >= E131_NETWORK_DATA_LOSS_TIMEOUT_MS)
    winner.has_runner_up = false;

  const bool terminated = info.options & E131_OPTION_STREAM_TERMINATED;
  if (winner.active && memcmp(info.cid, winner.cid, E131_CID_SIZE) == 0) {
    if (terminated) {
      // The winner stopped: network data loss at once, and its values are ignored (6.2.6).
      winner.active = false;
      memcpy(left_cid, winner.cid, E131_CID_SIZE);
      left_reason = "terminated its stream";
    } else {
      // Out of sequence (6.7.2): signed 8-bit difference in (-20, 0].
      const auto diff = static_cast<int8_t>(info.sequence - winner.sequence);
      if (diff > 0 || diff <= E131_SEQUENCE_WINDOW) {
        winner.priority = info.priority;
        winner.sequence = info.sequence;
        winner.last_packet_ms = now;
        decision = E131Decision::E131_DECISION_APPLY;
      }
    }
  } else if (terminated) {
    // Another source ends its stream: nothing to show.
  } else if (winner.active ? (info.priority < winner.priority ||
                              (info.priority == winner.priority && memcmp(info.cid, winner.cid, E131_CID_SIZE) > 0))
                           : (winner.has_runner_up && info.priority < winner.runner_up_priority)) {
    // Kept out: lower than the winner, or, after a handoff, lower than the best source kept out until then.
    if (winner.active && (!winner.has_runner_up || info.priority >= winner.runner_up_priority)) {
      winner.runner_up_priority = info.priority;
      winner.runner_up_ms = now;
      winner.has_runner_up = true;
    }
  } else {
    // Take over. A winner pushed aside becomes the runner-up, timed from its last packet, unless the runner-up
    // kept until then is higher, or equal and heard later; after a handoff that runner-up stays.
    if (winner.active &&
        (!winner.has_runner_up || winner.runner_up_priority < winner.priority ||
         (winner.runner_up_priority == winner.priority && now - winner.last_packet_ms < now - winner.runner_up_ms))) {
      winner.has_runner_up = true;
      winner.runner_up_priority = winner.priority;
      winner.runner_up_ms = winner.last_packet_ms;
    }
    memcpy(winner.cid, info.cid, E131_CID_SIZE);
    winner.priority = info.priority;
    winner.sequence = info.sequence;
    winner.last_packet_ms = now;
    winner.active = true;
    decision = E131Decision::E131_DECISION_TAKE_OVER;
  }

  if (left_reason != nullptr)
    this->log_source_leaving_(universe, left_cid, left_reason);
  if (decision == E131Decision::E131_DECISION_TAKE_OVER)
    this->log_source_taking_over_(universe, info);
  return decision;
}

// Logs only the first universe a source takes over, and only at debug level.
void E131Component::log_source_taking_over_(int universe, const E131SourceInfo &info) {
#ifdef ESPHOME_LOG_HAS_DEBUG
  if (this->has_logged_source_ && memcmp(this->logged_source_cid_, info.cid, E131_CID_SIZE) == 0)
    return;
  memcpy(this->logged_source_cid_, info.cid, E131_CID_SIZE);
  this->has_logged_source_ = true;
  ESP_LOGD(TAG, "Source '%.*s' (priority %u) takes over, from universe %d.",
           (int) strnlen(reinterpret_cast<const char *>(info.source_name), E131_SOURCE_NAME_SIZE),
           reinterpret_cast<const char *>(info.source_name), (unsigned) info.priority, universe);
#endif
}

// Logs only for the source last logged as taking over, once, and only at debug level.
void E131Component::log_source_leaving_(int universe, const uint8_t *cid, const char *reason) {
#ifdef ESPHOME_LOG_HAS_DEBUG
  if (!this->has_logged_source_ || memcmp(this->logged_source_cid_, cid, E131_CID_SIZE) != 0)
    return;
  this->has_logged_source_ = false;
  ESP_LOGD(TAG, "Source %s, from universe %d.", reason, universe);
#endif
}
#endif

bool E131Component::process_(int universe, const E131Packet &packet) {
  bool handled = false;

  ESP_LOGV(TAG, "Received E1.31 packet for %d universe, with %d bytes", universe, packet.count);

  for (auto *light_effect : light_effects_) {
    handled = light_effect->process_(universe, packet) || handled;
  }

  return handled;
}

}  // namespace esphome::e131

#endif
