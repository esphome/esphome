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
  // IP as text so the socket component's v4-mapped-IPv6 normalization is
  // reused on both ends; empty = none remembered
  char host[socket::SOCKADDR_STR_LEN];
} PACKED;  // NOLINT

/// Dials out when no dial-back target client is connected. Only the TCP
/// direction flips: the device stays the Noise responder, so both sides
/// still verify by key. Targets the YAML host or the last remembered client.
class OutgoingConnectionManager {
 public:
  void setup();
  void loop(APIServer *server);
  /// A key-verified client declared itself a dial-back target; last one wins
  void on_target_client(APIConnection *conn);
  /// Clears the dialed-connection gate; dying unproven escalates the backoff
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

  // Pointers first (4 bytes each on 32-bit)
  std::unique_ptr<socket::Socket> dial_socket_;
  // Compared only, never dereferenced
  APIConnection *dialed_conn_{nullptr};
#ifndef API_OUTGOING_CONNECTION_HOST
  ESPPreferenceObject target_pref_;
#endif

  // 4-byte types
  uint32_t backoff_{BACKOFF_MIN_MS};
  // Boot waits for the client to connect in first; a deep sleep wake window
  // is short, so connecting out immediately is the wake state
#ifdef USE_DEEP_SLEEP
  uint32_t wait_{0};
#else
  uint32_t wait_{API_OUTGOING_CONNECTION_DELAY};
#endif
  uint32_t state_ts_{0};
  uint32_t last_poll_{0};

  // Byte-aligned types last
#ifndef API_OUTGOING_CONNECTION_HOST
  SavedOutgoingTarget saved_{};
#endif
  DialState state_{DialState::DIAL_STATE_WAITING};
};

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
