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

// Follows the build's address family (ifdef'd in socket/headers.h): toggling
// enable_ipv6 changes the blob size, load() rejects the old blob, and the
// target is simply relearned
static constexpr size_t SAVED_TARGET_HOST_LEN = socket::SOCKADDR_STR_LEN;

struct SavedOutgoingTarget {
  // IP as text so the socket component's v4-mapped-IPv6 normalization is
  // reused on both ends; empty = none remembered
  char host[SAVED_TARGET_HOST_LEN];
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
  /// Clears the dialed-connection gate; dying unauthenticated escalates the backoff
  void on_client_removed(APIConnection *conn, bool was_authenticated);
  void on_shutdown() { this->dial_socket_.reset(); }
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
  static constexpr uint32_t NETWORK_RETRY_MS = 500;
  static constexpr uint32_t PRECONDITION_RETRY_MS = 5000;
  // Boot waits for the client to connect in first; a deep sleep wake window
  // is short, so connecting out immediately is the wake state
#ifdef USE_DEEP_SLEEP
  static constexpr uint32_t BOOT_WAIT_MS = 0;
#else
  static constexpr uint32_t BOOT_WAIT_MS = API_OUTGOING_CONNECTION_DELAY;
#endif

  void try_dial_(APIServer *server, uint32_t now);
  void poll_connect_(APIServer *server, uint32_t now);
  // Hand the connected socket to the server and gate on the new connection
  void handoff_(APIServer *server, uint32_t now);
  // Close any half-open dial and wait a jittered backoff before retrying
  void schedule_retry_(uint32_t now);
  // Wait without escalating the backoff (used for unmet preconditions)
  void schedule_wait_(uint32_t now, uint32_t wait);
  const char *target_host_() const {
#ifdef API_OUTGOING_CONNECTION_HOST
    return API_OUTGOING_CONNECTION_HOST;
#else
    return this->saved_.host[0] != '\0' ? this->saved_.host : nullptr;
#endif
  }

  // Pointers first (4 bytes each on 32-bit)
  std::unique_ptr<socket::Socket> dial_socket_;
  // Compared only, never dereferenced
  APIConnection *dialed_conn_{nullptr};
#ifndef API_OUTGOING_CONNECTION_HOST
  ESPPreferenceObject target_pref_;
#endif

  // 4-byte types
  uint32_t backoff_{BACKOFF_MIN_MS};
  uint32_t wait_{BOOT_WAIT_MS};
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
