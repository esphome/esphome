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
#include <cstring>

namespace esphome::api {

static const char *const TAG = "api.outgoing";

void OutgoingConnectionManager::setup() {
#ifndef API_OUTGOING_CONNECTION_HOST
  this->target_pref_ = global_preferences->make_preference<SavedOutgoingTarget>(629847102UL, true);
  if (this->target_pref_.load(&this->saved_)) {
    this->host_persisted_ = true;
    ESP_LOGD(TAG, "Loaded target %s", this->saved_.host);
  } else {
    // Never saved, or the blob failed its size/CRC check
    ESP_LOGD(TAG, "No saved target");
    this->saved_ = {};
  }
  // Defend against a corrupt or truncated preference blob
  this->saved_.host[sizeof(this->saved_.host) - 1] = '\0';
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
#ifdef USE_DEEP_SLEEP
      // A deep sleep wake window is too short to spend on the delay
      this->schedule_wait_(now, BACKOFF_MIN_MS);
#else
      // Target went away; give it the configured delay to reconnect first
      this->schedule_wait_(now, API_OUTGOING_CONNECTION_DELAY);
#endif
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
  const char *host = this->target_host_();
  if (host == nullptr) {
    // The steady state until a dial-back client has ever connected
    ESP_LOGV(TAG, "Not dialing: no target");
    this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    return;
  }
  const bool at_limit = server->at_client_limit_();
  if (at_limit || !server->noise_ctx_.has_psk()) {
    ESP_LOGD(TAG, "Not dialing: %s", at_limit ? "max connections" : "no key");
    // Not a dial failure; retry without escalating the backoff
    this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    return;
  }
  struct sockaddr_storage addr;
  socklen_t addr_len =
      socket::set_sockaddr((struct sockaddr *) &addr, sizeof(addr), host, API_OUTGOING_CONNECTION_PORT);
  if (addr_len == 0) {
    ESP_LOGW(TAG, "Invalid target %s", host);
#ifndef API_OUTGOING_CONNECTION_HOST
    // A corrupt remembered value can never become dialable; forget it
    // (covers an IPv6 literal left by an earlier enable_ipv6 build too)
    this->saved_ = {};
    if (!this->persist_target_()) {
      ESP_LOGW(TAG, "Failed to clear target");
    }
#endif
    this->schedule_retry_(now);
    return;
  }
  this->dial_socket_ = socket::socket_loop_monitored(((struct sockaddr *) &addr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (!this->dial_socket_ || this->dial_socket_->setblocking(false) != 0) {
    ESP_LOGW(TAG, "Socket %s failed: errno %d", this->dial_socket_ ? "setblocking" : "create", errno);
    this->schedule_retry_(now);
    return;
  }
  ESP_LOGD(TAG, "Dialing %s:%u", host, API_OUTGOING_CONNECTION_PORT);
  int err = this->dial_socket_->connect((struct sockaddr *) &addr, addr_len);
  if (err == 0) {
    // Immediate success (possible for localhost)
    this->handoff_(server, now);
    return;
  }
  if (errno != EINPROGRESS) {
    ESP_LOGW(TAG, "Connect failed: errno %d", errno);
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
    case socket::ConnectPollResult::CONNECT_POLL_PENDING:
      break;
    case socket::ConnectPollResult::CONNECT_POLL_CONNECTED:
      this->handoff_(server, now);
      break;
    case socket::ConnectPollResult::CONNECT_POLL_ERROR:
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
  const uint32_t now = App.get_loop_component_start_time();
  if (was_authenticated) {
    // A working peer (e.g. a host: target that never sends the flag)
    // disconnected normally; state is IDLE, so loop() applies the delay
    this->backoff_ = BACKOFF_MIN_MS;
  } else {
    this->schedule_retry_(now);
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
  conn->get_peername_to(target.host);
  if (target.host[0] == '\0') {
    ESP_LOGW(TAG, "Could not read peer address; not remembering target");
    return;
  }
  if (this->host_persisted_ && strcmp(target.host, this->saved_.host) == 0) {
    return;  // unchanged and already on flash; avoid flash wear
  }
  // Use the fresh address this boot even if the flash write fails; a failed
  // write is retried on the next flagged hello via host_persisted_
  this->saved_ = target;
  if (!this->persist_target_()) {
    ESP_LOGW(TAG, "Failed to save target");
    return;
  }
  ESP_LOGD(TAG, "Saved %s as outgoing connection target", this->saved_.host);
#endif
}

void OutgoingConnectionManager::dump_config() const {
  const char *host = this->target_host_();
  if (host == nullptr) {
    host = "none remembered yet";
  }
  // The boot delay differs from delay: on deep sleep builds, so print the
  // value that actually applies
  ESP_LOGCONFIG(TAG,
                "  Outgoing connection port: %u\n"
                "  Outgoing connection host: %s\n"
                "  Outgoing connection boot delay: %" PRIu32 "ms",
                API_OUTGOING_CONNECTION_PORT, host, BOOT_WAIT_MS);
}

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
