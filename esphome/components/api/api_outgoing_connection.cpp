#include "api_outgoing_connection.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#include "api_connection.h"
#include "api_server.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>

namespace esphome::api {

static const char *const TAG = "api.outgoing";

#ifndef API_OUTGOING_CONNECTION_HOST
static constexpr uint32_t OUTGOING_TARGET_PREF_HASH = 629847102UL;
#endif

#ifndef API_OUTGOING_CONNECTION_HOST
// Read the connection's peer address into target; false when unavailable or
// of a family this build cannot dial
static bool peer_to_target(APIConnection *conn, SavedOutgoingTarget &target) {
  // Zeroed because the raw lwIP getpeername() leaves sin6_scope_id untouched
  struct sockaddr_storage peer = {};
  socklen_t peer_len = sizeof(peer);
  if (conn->getpeername((struct sockaddr *) &peer, &peer_len) != 0) {
    return false;
  }
  const sa_family_t family = ((struct sockaddr *) &peer)->sa_family;
#if USE_NETWORK_IPV6
  if (family == AF_INET6) {
    const auto *addr6 = reinterpret_cast<const struct sockaddr_in6 *>(&peer);
    const auto *bytes = reinterpret_cast<const uint8_t *>(&addr6->sin6_addr);
    uint32_t prefix[3];
    memcpy(prefix, bytes, sizeof(prefix));
    // A dual-stack listener reports an IPv4 peer as ::ffff:a.b.c.d
    if (prefix[0] == 0 && prefix[1] == 0 && prefix[2] == htonl(0xFFFFUL)) {
      target.family = AF_INET;
      memcpy(target.addr, bytes + sizeof(prefix), sizeof(struct in_addr));
      return true;
    }
    // A link-local target is only reachable through the interface it came in
    // on. Device platforms number interfaces from one; a host build can hand
    // out an index too large to store, and a truncated one dials the wrong
    // interface, so that target is not remembered at all.
    if (addr6->sin6_scope_id > UINT8_MAX) {
      return false;
    }
    target.family = AF_INET6;
    memcpy(target.addr, bytes, sizeof(target.addr));
    target.scope_id = static_cast<uint8_t>(addr6->sin6_scope_id);
    return true;
  }
#endif
  if (family != AF_INET) {
    return false;
  }
  const auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(&peer);
  target.family = AF_INET;
  memcpy(target.addr, &addr4->sin_addr, sizeof(addr4->sin_addr));
  return true;
}
#endif

socklen_t OutgoingConnectionManager::target_sockaddr_(struct sockaddr_storage *addr) const {
#ifdef API_OUTGOING_CONNECTION_HOST
  // Validation only lets through a literal both inet_pton and inet6_aton
  // accept, so this cannot fail
  return socket::set_sockaddr((struct sockaddr *) addr, sizeof(*addr), API_OUTGOING_CONNECTION_HOST,
                              API_OUTGOING_CONNECTION_PORT);
#else
#if USE_NETWORK_IPV6
  if (this->saved_.family == AF_INET6) {
    auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(addr);
    memset(addr6, 0, sizeof(*addr6));
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = htons(API_OUTGOING_CONNECTION_PORT);
    memcpy(&addr6->sin6_addr, this->saved_.addr, sizeof(this->saved_.addr));
    addr6->sin6_scope_id = this->saved_.scope_id;
    return sizeof(*addr6);
  }
#endif
  if (this->saved_.family != AF_INET) {
    return 0;
  }
  auto *addr4 = reinterpret_cast<struct sockaddr_in *>(addr);
  memset(addr4, 0, sizeof(*addr4));
  addr4->sin_family = AF_INET;
  addr4->sin_port = htons(API_OUTGOING_CONNECTION_PORT);
  memcpy(&addr4->sin_addr, this->saved_.addr, sizeof(addr4->sin_addr));
  return sizeof(*addr4);
#endif
}

#ifndef API_OUTGOING_CONNECTION_HOST
void OutgoingConnectionManager::format_target_(std::span<char, socket::SOCKADDR_STR_LEN> buf) const {
  struct sockaddr_storage addr;
  socklen_t addr_len = this->target_sockaddr_(&addr);
  if (addr_len == 0) {
    buf[0] = '\0';
    return;
  }
  // Clears buf itself if it cannot format the address
  socket::format_sockaddr_to((struct sockaddr *) &addr, addr_len, buf);
}
#endif

void OutgoingConnectionManager::setup() {
#ifndef API_OUTGOING_CONNECTION_HOST
  this->target_pref_ = global_preferences->make_preference<SavedOutgoingTarget>(OUTGOING_TARGET_PREF_HASH, true);
  struct sockaddr_storage addr;
  // dump_config() prints whichever target this leaves in place
  if (this->target_pref_.load(&this->saved_) && this->target_sockaddr_(&addr) != 0) {
    this->host_persisted_ = true;
  } else {
    // Never saved, failed its size or CRC check, or holds an unknown family
    this->saved_ = {};
  }
#endif
}

void OutgoingConnectionManager::loop(APIServer *server) {
  if (server->has_outgoing_target_client_()) {
    return;  // on_target_client() already reset the dial state
  }
  if (this->dialed_conn_ != nullptr) {
    // A live dialed session (flagged or not, e.g. a host: peer) is the
    // target; a silent one dies on the handshake timeout
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  switch (this->state_) {
    case DialState::DIAL_STATE_IDLE:
      // Target went away; give it the configured delay to reconnect first
      this->schedule_wait_(now, IDLE_WAIT_MS);
      break;
    case DialState::DIAL_STATE_WAITING:
      if (now - this->state_ts_ >= this->wait_) {
        this->try_dial_(server, now);
      }
      break;
    case DialState::DIAL_STATE_CONNECTING:
      this->poll_connect_(server, now);
      break;
  }
}

void OutgoingConnectionManager::try_dial_(APIServer *server, uint32_t now) {
  if (!network::is_connected()) {
    // Flips within seconds of boot; recheck fast so a deep sleep wake
    // window is not spent waiting
    this->schedule_wait_(now, NETWORK_RETRY_MS);
    return;
  }
  struct sockaddr_storage addr;
  socklen_t addr_len = this->target_sockaddr_(&addr);
  const bool at_limit = server->at_client_limit_();
  // No target is the steady state until a dial-back client has ever connected
  if (addr_len == 0 || at_limit || !server->noise_ctx_.has_psk()) {
    // Repeats for as long as the reason holds, so keep it out of debug logs
    ESP_LOGV(TAG, "Not dialing: %s",
             addr_len == 0 ? LOG_STR_LITERAL("no target")
                           : (at_limit ? LOG_STR_LITERAL("max connections") : LOG_STR_LITERAL("no key")));
    // Not a dial failure; retry without escalating the backoff
    this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    return;
  }
  this->dial_socket_ = socket::socket_loop_monitored(((struct sockaddr *) &addr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (!this->dial_socket_ || this->dial_socket_->setblocking(false) != 0) {
    ESP_LOGW(TAG, "Socket %s failed: errno %d",
             this->dial_socket_ ? LOG_STR_LITERAL("setblocking") : LOG_STR_LITERAL("create"), errno);
    this->schedule_retry_(now);
    return;
  }
#ifdef API_OUTGOING_CONNECTION_HOST
  ESP_LOGD(TAG, "Dialing " API_OUTGOING_CONNECTION_HOST ":%u", API_OUTGOING_CONNECTION_PORT);
#else
  char host[socket::SOCKADDR_STR_LEN];
  socket::format_sockaddr_to((struct sockaddr *) &addr, addr_len, host);
  ESP_LOGD(TAG, "Dialing %s:%u", host, API_OUTGOING_CONNECTION_PORT);
#endif
  int err = this->dial_socket_->connect((struct sockaddr *) &addr, addr_len);
  if (err == 0) {
    // Immediate success (possible for localhost)
    this->handoff_(server, now);
    return;
  }
  if (errno != EINPROGRESS) {
    ESP_LOGW(TAG, "Connect failed: %d", errno);
    this->schedule_retry_(now);
    return;
  }
  this->state_ = DialState::DIAL_STATE_CONNECTING;
  this->state_ts_ = now;
  this->last_poll_ = now;
}

void OutgoingConnectionManager::poll_connect_(APIServer *server, uint32_t now) {
  if (now - this->state_ts_ >= CONNECT_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Connect timeout");
    this->schedule_retry_(now);
    return;
  }
  if (now - this->last_poll_ < CONNECT_POLL_INTERVAL_MS) {
    return;
  }
  this->last_poll_ = now;
  int err = 0;
  switch (socket::poll_connect(*this->dial_socket_, err)) {
    case socket::ConnectPollResult::CONNECT_POLL_RESULT_PENDING:
      break;
    case socket::ConnectPollResult::CONNECT_POLL_RESULT_CONNECTED:
      this->handoff_(server, now);
      break;
    case socket::ConnectPollResult::CONNECT_POLL_RESULT_ERROR:
      ESP_LOGW(TAG, "Connect failed: %d", err);
      this->schedule_retry_(now);
      break;
  }
}

void OutgoingConnectionManager::handoff_(APIServer *server, uint32_t now) {
  this->dialed_conn_ = server->add_outgoing_client_(std::move(this->dial_socket_));
  if (this->dialed_conn_ == nullptr) {
    // Only preconditions (slot limit, key cleared) refuse the handoff; the
    // peer is reachable, so do not escalate the backoff
    this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    return;
  }
  // Connected; dialed_conn_ gates further dialing until the session settles
  this->state_ = DialState::DIAL_STATE_IDLE;
}

void OutgoingConnectionManager::schedule_wait_(uint32_t now, uint32_t wait) {
  this->dial_socket_.reset();  // no-op when the socket was handed off
  this->state_ = DialState::DIAL_STATE_WAITING;
  this->state_ts_ = now;
  this->wait_ = wait;
}

void OutgoingConnectionManager::schedule_retry_(uint32_t now) {
  // +/-20% jitter so a fleet of devices does not retry one server in lockstep
  const uint32_t jitter_span = this->backoff_ / 5;
  this->schedule_wait_(now, this->backoff_ - jitter_span + (random_uint32() % (2 * jitter_span + 1)));
  this->backoff_ = std::min(this->backoff_ * 2, BACKOFF_MAX_MS);
}

void OutgoingConnectionManager::on_client_removed(APIConnection *conn, bool was_authenticated) {
  if (conn != this->dialed_conn_) {
    return;
  }
  this->dialed_conn_ = nullptr;
  if (was_authenticated) {
    // A working peer (e.g. a host: target that never sends the flag)
    // disconnected normally; state is IDLE, so loop() applies the delay
    this->backoff_ = BACKOFF_MIN_MS;
  } else {
    this->schedule_retry_(App.get_loop_component_start_time());
  }
}

void OutgoingConnectionManager::on_target_client(APIConnection *conn) {
  // The target is connected; stop any dial in flight and reset the backoff.
  // A dialed connection stays tracked unless it is this one: an inbound
  // target must not orphan a still-open dial.
  this->dial_socket_.reset();
  if (conn == this->dialed_conn_) {
    this->dialed_conn_ = nullptr;
  }
  this->state_ = DialState::DIAL_STATE_IDLE;
  this->backoff_ = BACKOFF_MIN_MS;
#ifndef API_OUTGOING_CONNECTION_HOST
  SavedOutgoingTarget target{};
  if (!peer_to_target(conn, target)) {
    ESP_LOGW(TAG, "Not remembering this target; its address cannot be dialed");
    return;
  }
  if (this->host_persisted_ && memcmp(&target, &this->saved_, sizeof(target)) == 0) {
    return;  // unchanged and already on flash; avoid flash wear
  }
  // Use the fresh address this boot even if the flash write fails; a failed
  // write is retried on the next flagged hello via host_persisted_
  this->saved_ = target;
  if (!this->persist_target_()) {
    ESP_LOGW(TAG, "Failed to save target");
    return;
  }
  char host[socket::SOCKADDR_STR_LEN];
  this->format_target_(host);
  ESP_LOGD(TAG, "Remembered %s as the dial target", host);
#endif
}

void OutgoingConnectionManager::dump_config() const {
  // The boot delay differs from delay: on deep sleep builds, so print the
  // value that actually applies
  ESP_LOGCONFIG(TAG,
                "  Outgoing connection port: %u\n"
                "  Outgoing connection boot delay: %" PRIu32 "ms",
                API_OUTGOING_CONNECTION_PORT, BOOT_WAIT_MS);
  // Both forms keep their text out of RAM on ESP8266: in the format string,
  // or through LOG_STR_LITERAL
#ifdef API_OUTGOING_CONNECTION_HOST
  ESP_LOGCONFIG(TAG, "  Outgoing connection host: " API_OUTGOING_CONNECTION_HOST);
#else
  char buf[socket::SOCKADDR_STR_LEN];
  this->format_target_(buf);
  ESP_LOGCONFIG(TAG, "  Outgoing connection host: %s", buf[0] == '\0' ? LOG_STR_LITERAL("none remembered yet") : buf);
#endif
}

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
