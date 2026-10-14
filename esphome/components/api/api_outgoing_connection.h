#pragma once

#include "esphome/core/defines.h"
#if defined(USE_API) && defined(USE_API_OUTGOING_CONNECTION)

#ifndef USE_API_NOISE
#error "api outgoing_connection needs noise encryption so the peer is verified by key"
#endif

#include "esphome/components/socket/socket.h"
#include "esphome/core/preferences.h"

#include <memory>

namespace esphome::api {

class APIServer;
class APIConnection;

// Room for an IPv6 address in every build, so a remembered IPv4 target is
// still dialed after enable_ipv6 is turned on. A size that followed the build
// would also shift every preference registered after this one on ESP8266,
// where slots are positional. An IPv6 target on a build without IPv6 is
// dropped by target_sockaddr_() and relearned.
// Bytes in an IPv6 address
static constexpr size_t TARGET_ADDR_LEN = 16;

struct SavedOutgoingTarget {
  // 0 when none is remembered, else AF_INET or AF_INET6
  uint8_t family;
  // Network order, IPv4 in the first four bytes and the rest zero
  uint8_t addr[TARGET_ADDR_LEN];
  // Interface a link-local IPv6 target is reachable on, 0 when it needs none.
  // Free in flash: the record still rounds up to the same five words.
  uint8_t scope_id;
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
  // A deep sleep wake window is too short to spend on the delay, so those
  // builds dial out as soon as the target is gone
#ifdef USE_DEEP_SLEEP
  static constexpr uint32_t BOOT_WAIT_MS = 0;
  static constexpr uint32_t IDLE_WAIT_MS = BACKOFF_MIN_MS;
#else
  static constexpr uint32_t BOOT_WAIT_MS = API_OUTGOING_CONNECTION_DELAY;
  static constexpr uint32_t IDLE_WAIT_MS = API_OUTGOING_CONNECTION_DELAY;
#endif

  void try_dial_(APIServer *server, uint32_t now);
  void poll_connect_(APIServer *server, uint32_t now);
  // Hand the connected socket to the server and gate on the new connection
  void handoff_(APIServer *server, uint32_t now);
  // Close any half-open dial and wait a jittered backoff before retrying
  void schedule_retry_(uint32_t now);
  // Wait without escalating the backoff (used for unmet preconditions)
  void schedule_wait_(uint32_t now, uint32_t wait);
  /// Fill addr with the target and return its length, or 0 when there is none
  socklen_t target_sockaddr_(struct sockaddr_storage *addr) const;
#ifndef API_OUTGOING_CONNECTION_HOST
  // Write saved_ to flash, tracking success in host_persisted_
  bool persist_target_() {
    this->host_persisted_ = this->target_pref_.save(&this->saved_) && global_preferences->sync();
    return this->host_persisted_;
  }
  /// Format the remembered target for a log line; empty when there is none
  void format_target_(std::span<char, socket::SOCKADDR_STR_LEN> buf) const;
#endif

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
  // False while saved_ holds a value the flash write failed for; retried on
  // the next flagged hello
  bool host_persisted_{false};
#endif
  DialState state_{DialState::DIAL_STATE_WAITING};
};

}  // namespace esphome::api
#endif  // USE_API && USE_API_OUTGOING_CONNECTION
