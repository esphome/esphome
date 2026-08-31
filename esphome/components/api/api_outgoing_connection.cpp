#include "api_outgoing_connection.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#include "api_connection.h"
#include "api_server.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>
#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
#include <sys/select.h>
#endif

namespace esphome::api {

static const char *const TAG = "api.outgoing";

void OutgoingConnectionManager::setup() {
#ifndef API_OUTGOING_CONNECTION_HOST
  this->target_pref_ = global_preferences->make_preference<SavedOutgoingTarget>(629847102UL, true);
  if (this->target_pref_.load(&this->saved_)) {
    // Defend against a corrupt or truncated preference blob
    this->saved_.host[socket::SOCKADDR_STR_LEN - 1] = '\0';
  } else {
    this->saved_.host[0] = '\0';
  }
#endif
}

void OutgoingConnectionManager::loop(APIServer *server) {
  if (server->has_outgoing_target_client_()) {
    // on_target_client() already reset the dial state when this client's
    // hello arrived; nothing to do while it stays connected.
    return;
  }
  if (this->dialed_conn_ != nullptr) {
    // A dialed connection is still open but its peer has not sent a flagged
    // hello (yet); dialing again would only burn connection slots. The
    // connection's own timeouts remove it eventually if the peer is silent.
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  switch (this->state_) {
    case DialState::DIAL_STATE_IDLE:
      // The connected target went away; give it the configured delay to
      // reconnect on its own before dialing.
      this->state_ = DialState::DIAL_STATE_WAITING;
      this->state_ts_ = now;
      this->wait_ = API_OUTGOING_CONNECTION_DELAY;
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
  const char *host = this->target_host_();
  if (host == nullptr || !network::is_connected() || server->at_client_limit_() || !server->noise_ctx_.has_psk()) {
    // Not a dial failure; retry soon without escalating the backoff
    this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    return;
  }
  struct sockaddr_storage addr;
  socklen_t addr_len =
      socket::set_sockaddr((struct sockaddr *) &addr, sizeof(addr), host, API_OUTGOING_CONNECTION_PORT);
  if (addr_len == 0) {
    ESP_LOGW(TAG, "Invalid target %s", host);
    this->schedule_retry_(now);
    return;
  }
  this->dial_socket_ = socket::socket_loop_monitored(((struct sockaddr *) &addr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (!this->dial_socket_ || this->dial_socket_->setblocking(false) != 0) {
    ESP_LOGW(TAG, "Socket create failed: errno %d", errno);
    this->schedule_retry_(now);
    return;
  }
  ESP_LOGD(TAG, "Dialing %s:%u", host, API_OUTGOING_CONNECTION_PORT);
  int err = this->dial_socket_->connect((struct sockaddr *) &addr, addr_len);
  if (err == 0) {
    // Immediate success (possible for localhost)
    this->dialed_conn_ = server->add_outgoing_client_(std::move(this->dial_socket_));
    if (this->dialed_conn_ == nullptr) {
      this->schedule_retry_(now);
    } else {
      this->schedule_wait_(now, PRECONDITION_RETRY_MS);
    }
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
  int fd = this->dial_socket_->get_fd();
  if (fd < 0 || fd >= FD_SETSIZE) {
    ESP_LOGW(TAG, "Bad fd for connect poll: %d", fd);
    this->schedule_retry_(now);
    return;
  }
  // Connect completion is a write event; the main loop select() only watches
  // read readiness, so poll it here with a zero timeout.
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(fd, &writefds);
  struct timeval tv = {0, 0};
#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS
  // LWIP_COMPAT_SOCKETS may be off (LibreTiny), so use the lwip symbol directly
  int ret = lwip_select(fd + 1, nullptr, &writefds, nullptr, &tv);
#else
  // Global-scope select: the entity namespace esphome::select shadows it here
  int ret = ::select(fd + 1, nullptr, &writefds, nullptr, &tv);
#endif
  if (ret < 0) {
    ESP_LOGW(TAG, "Connect poll failed: errno %d", errno);
    this->schedule_retry_(now);
    return;
  }
  if (ret == 0 || !FD_ISSET(fd, &writefds)) {
    return;  // still in progress
  }
  int error = 0;
  socklen_t len = sizeof(error);
  if (this->dial_socket_->getsockopt(SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
    ESP_LOGW(TAG, "Connect failed: %d", error);
    this->schedule_retry_(now);
    return;
  }
  this->dialed_conn_ = server->add_outgoing_client_(std::move(this->dial_socket_));
  if (this->dialed_conn_ == nullptr) {
    this->schedule_retry_(now);
    return;
  }
  // The dial worked; hold without escalating until the peer proves itself
  // with a flagged hello (on_target_client() resets to idle) or dies
  // unproven (on_client_removed() escalates the backoff).
  this->schedule_wait_(now, PRECONDITION_RETRY_MS);
}

void OutgoingConnectionManager::schedule_retry_(uint32_t now) {
  this->dial_socket_.reset();  // no-op when the socket was handed off
  this->state_ = DialState::DIAL_STATE_WAITING;
  this->state_ts_ = now;
  // +/-20% jitter so a fleet of devices does not retry one server in lockstep
  const uint32_t jitter_span = this->backoff_ / 5;
  this->wait_ = this->backoff_ - jitter_span + (random_uint32() % (2 * jitter_span + 1));
  this->backoff_ = std::min(this->backoff_ * 2, BACKOFF_MAX_MS);
}

void OutgoingConnectionManager::on_client_removed(APIConnection *conn) {
  if (conn == this->dialed_conn_) {
    this->dialed_conn_ = nullptr;
    this->schedule_retry_(App.get_loop_component_start_time());
  }
}

void OutgoingConnectionManager::on_target_client(APIConnection *conn) {
  // The target is connected; stop any dial in flight and reset the backoff.
  this->dial_socket_.reset();
  this->dialed_conn_ = nullptr;
  this->state_ = DialState::DIAL_STATE_IDLE;
  this->backoff_ = BACKOFF_MIN_MS;
#ifndef API_OUTGOING_CONNECTION_HOST
  SavedOutgoingTarget target{};
  conn->get_peername_to(target.host);
  if (target.host[0] == '\0' || strcmp(target.host, this->saved_.host) == 0) {
    return;  // unknown peer or unchanged; avoid flash wear
  }
  if (!this->target_pref_.save(&target) || !global_preferences->sync()) {
    // Keep the old value so the save is retried on the next flagged hello
    ESP_LOGW(TAG, "Failed to save target");
    return;
  }
  this->saved_ = target;
  ESP_LOGD(TAG, "Saved %s as outgoing connection target", this->saved_.host);
#endif
}

void OutgoingConnectionManager::dump_config() const {
  ESP_LOGCONFIG(TAG, "  Outgoing connection port: %u", API_OUTGOING_CONNECTION_PORT);
#ifdef API_OUTGOING_CONNECTION_HOST
  ESP_LOGCONFIG(TAG, "  Outgoing connection host: %s", API_OUTGOING_CONNECTION_HOST);
#else
  if (this->saved_.host[0] != '\0') {
    ESP_LOGCONFIG(TAG, "  Outgoing connection host: %s (remembered)", this->saved_.host);
  } else {
    ESP_LOGCONFIG(TAG, "  Outgoing connection host: none remembered yet");
  }
#endif
}

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
