#include "api_outgoing_connection.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#include "api_frame_helper.h"
#include "api_server.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>

namespace esphome::api {

static const char *const TAG = "api.outgoing";

void OutgoingConnectionManager::setup() {
  this->target_pref_ = global_preferences->make_preference<SavedOutgoingTarget>(629847102UL, true);
  if (this->target_pref_.load(&this->saved_)) {
    // Defend against a corrupt or truncated preference blob
    this->saved_.host[socket::SOCKADDR_STR_LEN - 1] = '\0';
  } else {
    this->saved_.host[0] = '\0';
  }
}

void OutgoingConnectionManager::loop(APIServer *server) {
  const uint32_t now = App.get_loop_component_start_time();
  const bool has_subscriber = server->is_connected_with_state_subscription();
  if (has_subscriber) {
    if (this->state_ != DialState::DIAL_STATE_IDLE) {
      this->abort_dial_();
      this->state_ = DialState::DIAL_STATE_IDLE;
      this->backoff_ = BACKOFF_MIN_MS;
    }
    return;
  }
  switch (this->state_) {
    case DialState::DIAL_STATE_IDLE:
      this->state_ = DialState::DIAL_STATE_WAITING;
      this->state_ts_ = now;
      break;
    case DialState::DIAL_STATE_WAITING:
      if (now - this->state_ts_ >= this->delay_) {
        this->try_dial_(server, now);
      }
      break;
    case DialState::DIAL_STATE_CONNECTING:
      this->poll_connect_(server, now);
      break;
    case DialState::DIAL_STATE_COOLDOWN:
      if (now - this->state_ts_ >= this->cooldown_wait_) {
        this->try_dial_(server, now);
      }
      break;
  }
}

void OutgoingConnectionManager::try_dial_(APIServer *server, uint32_t now) {
  const char *host = this->target_host_();
  if (host == nullptr || !network::is_connected() || server->api_connection_count_ >= MAX_API_CONNECTIONS ||
      !server->noise_ctx_.has_psk()) {
    this->enter_cooldown_(now);
    return;
  }
  struct sockaddr_storage addr {};
  socklen_t addr_len = socket::set_sockaddr((struct sockaddr *) &addr, sizeof(addr), host, this->port_);
  if (addr_len == 0) {
    ESP_LOGW(TAG, "Invalid outgoing connection target %s", host);
    this->enter_cooldown_(now);
    return;
  }
  this->dial_socket_ = socket::socket_loop_monitored(((struct sockaddr *) &addr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (!this->dial_socket_ || this->dial_socket_->setblocking(false) != 0) {
    this->abort_dial_();
    this->enter_cooldown_(now);
    return;
  }
  ESP_LOGD(TAG, "Dialing %s:%u", host, this->port_);
  int err = this->dial_socket_->connect((struct sockaddr *) &addr, addr_len);
  if (err == 0) {
    // Immediate success (possible for localhost)
    server->add_outgoing_client_(std::move(this->dial_socket_));
    this->enter_cooldown_(now);
    return;
  }
  if (errno != EINPROGRESS) {
    ESP_LOGW(TAG, "Outgoing connect failed: errno %d", errno);
    this->abort_dial_();
    this->enter_cooldown_(now);
    return;
  }
  this->state_ = DialState::DIAL_STATE_CONNECTING;
  this->state_ts_ = now;
}

void OutgoingConnectionManager::poll_connect_(APIServer *server, uint32_t now) {
  if (now - this->state_ts_ >= CONNECT_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Outgoing connect timeout");
    this->abort_dial_();
    this->enter_cooldown_(now);
    return;
  }
  int fd = this->dial_socket_->get_fd();
  if (fd < 0) {
    this->abort_dial_();
    this->enter_cooldown_(now);
    return;
  }
  // Connect completion is a write event; the main loop select() only watches
  // read readiness, so poll it here with a zero timeout.
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(fd, &writefds);
  struct timeval tv = {0, 0};
  int ret = select(fd + 1, nullptr, &writefds, nullptr, &tv);
  if (ret <= 0 || !FD_ISSET(fd, &writefds)) {
    return;  // still in progress
  }
  int error = 0;
  socklen_t len = sizeof(error);
  if (this->dial_socket_->getsockopt(SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
    ESP_LOGW(TAG, "Outgoing connect failed: %d", error);
    this->abort_dial_();
    this->enter_cooldown_(now);
    return;
  }
  server->add_outgoing_client_(std::move(this->dial_socket_));
  // Stay in cooldown until the peer proves itself by subscribing to states;
  // loop() flips back to idle and resets the backoff when that happens.
  this->enter_cooldown_(now);
}

void OutgoingConnectionManager::enter_cooldown_(uint32_t now) {
  this->state_ = DialState::DIAL_STATE_COOLDOWN;
  this->state_ts_ = now;
  // +/-20% jitter so a fleet of devices does not retry one server in lockstep
  const uint32_t jitter_span = this->backoff_ / 5;
  this->cooldown_wait_ = this->backoff_ - jitter_span + (random_uint32() % (2 * jitter_span + 1));
  this->backoff_ = std::min(this->backoff_ * 2, BACKOFF_MAX_MS);
}

void OutgoingConnectionManager::on_state_subscription(const char *client_name, APIFrameHelper *helper) {
  // A state subscriber is connected; any dial in flight is now pointless.
  this->abort_dial_();
  this->state_ = DialState::DIAL_STATE_IDLE;
  this->backoff_ = BACKOFF_MIN_MS;
  if (strncmp(client_name, "Home Assistant", 14) != 0) {
    return;
  }
  struct sockaddr_storage peer {};
  socklen_t peer_len = sizeof(peer);
  if (helper->getpeername((struct sockaddr *) &peer, &peer_len) != 0) {
    return;
  }
  SavedOutgoingTarget target{};
  if (socket::format_sockaddr_to((struct sockaddr *) &peer, peer_len, target.host) == 0) {
    return;
  }
  if (strcmp(target.host, this->saved_.host) == 0) {
    return;  // unchanged; avoid flash wear
  }
  this->saved_ = target;
  if (!this->target_pref_.save(&this->saved_) || !global_preferences->sync()) {
    ESP_LOGW(TAG, "Failed to save outgoing connection target");
    return;
  }
  ESP_LOGD(TAG, "Saved %s as outgoing connection target", this->saved_.host);
}

void OutgoingConnectionManager::dump_config() const {
  ESP_LOGCONFIG(TAG, "  Outgoing connection port: %u", this->port_);
  if (this->configured_host_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Outgoing connection host: %s", this->configured_host_);
  } else if (this->saved_.host[0] != '\0') {
    ESP_LOGCONFIG(TAG, "  Outgoing connection host: %s (remembered)", this->saved_.host);
  }
}

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
