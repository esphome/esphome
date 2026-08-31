#pragma once

#include "esphome/core/defines.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#error "api outgoing_connection needs a socket implementation that can make outgoing connections"
#endif

#include "esphome/components/socket/socket.h"
#include "esphome/core/preferences.h"

#include <memory>

namespace esphome::api {

class APIServer;
class APIFrameHelper;

struct SavedOutgoingTarget {
  // Null-terminated IP string; empty when no peer has been remembered yet.
  // Stored as text so the platform-specific v4-mapped-IPv6 normalization in
  // socket::format_sockaddr_to()/set_sockaddr() is reused on both ends.
  char host[socket::SOCKADDR_STR_LEN];
} PACKED;  // NOLINT

/// Dials out to Home Assistant when no state-subscribed client is connected.
/// The TCP direction flips but the protocol roles do not: this device stays the
/// Noise responder, so both sides still verify each other by the shared key.
/// The target is either a host from YAML or the last persisted Home Assistant
/// peer address; the listening socket keeps accepting inbound clients the
/// whole time.
class OutgoingConnectionManager {
 public:
  void setup();
  void loop(APIServer *server);
  /// Called when a client subscribes to states. Home Assistant peers are
  /// persisted as the dial-back target; any subscriber cancels dialing.
  void on_state_subscription(const char *client_name, APIFrameHelper *helper);
  void on_shutdown() { this->abort_dial_(); }
  void set_target_host(const char *host) { this->configured_host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_delay(uint32_t delay) { this->delay_ = delay; }
  void dump_config() const;

 protected:
  enum class DialState : uint8_t {
    DIAL_STATE_IDLE,
    DIAL_STATE_WAITING,
    DIAL_STATE_CONNECTING,
    DIAL_STATE_COOLDOWN,
  };

  static constexpr uint32_t BACKOFF_MIN_MS = 5000;
  static constexpr uint32_t BACKOFF_MAX_MS = 300000;
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;

  void try_dial_(APIServer *server, uint32_t now);
  void poll_connect_(APIServer *server, uint32_t now);
  void abort_dial_() { this->dial_socket_.reset(); }
  void enter_cooldown_(uint32_t now);
  const char *target_host_() const {
    if (this->configured_host_ != nullptr)
      return this->configured_host_;
    return this->saved_.host[0] != '\0' ? this->saved_.host : nullptr;
  }

  std::unique_ptr<socket::Socket> dial_socket_;
  const char *configured_host_{nullptr};
  ESPPreferenceObject target_pref_;
  SavedOutgoingTarget saved_{};
  uint32_t delay_{60000};
  uint32_t backoff_{BACKOFF_MIN_MS};
  uint32_t cooldown_wait_{0};
  uint32_t state_ts_{0};
  uint16_t port_{6054};
  DialState state_{DialState::DIAL_STATE_IDLE};
};

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
