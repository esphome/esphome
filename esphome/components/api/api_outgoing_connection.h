#pragma once

#include "esphome/core/defines.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#error "api outgoing_connection needs a socket implementation that can make outgoing connections"
#endif
#ifndef USE_API_NOISE
#error "api outgoing_connection needs noise encryption so the peer is verified by key"
#endif

#include "esphome/components/socket/socket.h"
#include "esphome/core/preferences.h"

#include <memory>

namespace esphome::api {

class APIServer;
class APIConnection;

struct SavedOutgoingTarget {
  // Null-terminated IP string; empty when no peer has been remembered yet.
  // Stored as text so the platform-specific v4-mapped-IPv6 normalization in
  // the socket component is reused on both ends.
  char host[socket::SOCKADDR_STR_LEN];
} PACKED;  // NOLINT

/// Dials out to Home Assistant when no dial-back target client is connected.
/// The TCP direction flips but the protocol roles do not: this device stays
/// the Noise responder, so both sides still verify each other by the shared
/// key. The target is either a host from YAML or the persisted address of the
/// last client whose hello declared it a dial-back target; the listening
/// socket keeps accepting inbound clients the whole time.
class OutgoingConnectionManager {
 public:
  void setup();
  void loop(APIServer *server);
  /// Called when a key-verified client declares itself a dial-back target;
  /// the last such client wins as the remembered address.
  void on_target_client(APIConnection *conn);
  /// Called for every removed connection so a dialed one stops gating
  /// re-dials. A dialed connection dying without ever sending the flagged
  /// hello is the unproven-peer case, so the backoff escalates here.
  void on_client_removed(APIConnection *conn);
  void on_shutdown() {
    this->dial_socket_.reset();
    this->state_ = DialState::DIAL_STATE_IDLE;
  }
  void dump_config() const;

 protected:
  enum class DialState : uint8_t {
    DIAL_STATE_IDLE,
    DIAL_STATE_WAITING,
    DIAL_STATE_CONNECTING,
  };

  static constexpr uint32_t BACKOFF_MIN_MS = 5000;
  static constexpr uint32_t BACKOFF_MAX_MS = 300000;
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
  static constexpr uint32_t CONNECT_POLL_INTERVAL_MS = 250;

  void try_dial_(APIServer *server, uint32_t now);
  void poll_connect_(APIServer *server, uint32_t now);
  // Close any half-open dial and wait a jittered backoff before retrying
  void schedule_retry_(uint32_t now);
  // Wait without escalating the backoff (used for unmet preconditions)
  void schedule_wait_(uint32_t now, uint32_t wait) {
    this->dial_socket_.reset();
    this->state_ = DialState::DIAL_STATE_WAITING;
    this->state_ts_ = now;
    this->wait_ = wait;
  }
  const char *target_host_() const {
#ifdef API_OUTGOING_CONNECTION_HOST
    return API_OUTGOING_CONNECTION_HOST;
#else
    return this->saved_.host[0] != '\0' ? this->saved_.host : nullptr;
#endif
  }
  static constexpr uint32_t PRECONDITION_RETRY_MS = 5000;

  std::unique_ptr<socket::Socket> dial_socket_;
  // Connection created by the last successful dial; compared, never
  // dereferenced. Cleared by on_client_removed()/on_target_client().
  APIConnection *dialed_conn_{nullptr};
#ifndef API_OUTGOING_CONNECTION_HOST
  ESPPreferenceObject target_pref_;
  SavedOutgoingTarget saved_{};
#endif
  uint32_t backoff_{BACKOFF_MIN_MS};
  uint32_t wait_{0};
  uint32_t state_ts_{0};
  uint32_t last_poll_{0};
  // Boot starts in WAITING with wait_ 0: when no dial-back client has ever
  // connected (a device its client could never reach), dial immediately.
  // The configured delay only applies after a connected target goes away.
  DialState state_{DialState::DIAL_STATE_WAITING};
};

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
