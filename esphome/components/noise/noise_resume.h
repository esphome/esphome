#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NOISE
#include <cstddef>
#include <cstdint>

// Forward declaration matching <noise/protocol/cipherstate.h>; keeps noise-c
// headers out of everything that includes noise.h.
extern "C" {
typedef struct NoiseCipherState_s NoiseCipherState;  // NOLINT(modernize-use-using)
}

namespace esphome::noise {

/** Session resume for the noise transports.
 *
 * After a full handshake the responder issues a single-use ticket over the
 * encrypted channel. A client presents it in its next ClientHello and both
 * sides derive the transport keys with HKDF-SHA256 alone, skipping the two
 * curve25519 operations. Old peers ignore the extension bytes on both
 * sides, so every mismatch degrades to a normal full handshake.
 *
 * HKDF is the Noise construction (noise_hashstate_hkdf). Derivations:
 *   offer_mac   = HKDF(secret, "offer"   || session_id || client_nonce).out1[:16]
 *   confirm_mac = HKDF(secret, "confirm" || client_nonce || server_nonce).out1[:16]
 *   k_c2d, k_d2c = HKDF(secret, "keys" || client_nonce || server_nonce || SHA256(prologue))
 */

static constexpr uint8_t RESUME_OFFER_VERSION = 0x01;
static constexpr uint8_t RESUME_ACCEPT_VERSION = 0x01;
static constexpr size_t RESUME_SESSION_ID_SIZE = 8;
static constexpr size_t RESUME_NONCE_SIZE = 16;
static constexpr size_t RESUME_MAC_SIZE = 16;
static constexpr size_t RESUME_SECRET_SIZE = 32;

// ClientHello body: version | session_id | client_nonce | offer_mac
static constexpr size_t RESUME_OFFER_SIZE = 1 + RESUME_SESSION_ID_SIZE + RESUME_NONCE_SIZE + RESUME_MAC_SIZE;  // 41
static constexpr size_t RESUME_OFFER_SESSION_ID_OFFSET = 1;
static constexpr size_t RESUME_OFFER_NONCE_OFFSET = RESUME_OFFER_SESSION_ID_OFFSET + RESUME_SESSION_ID_SIZE;
static constexpr size_t RESUME_OFFER_MAC_OFFSET = RESUME_OFFER_NONCE_OFFSET + RESUME_NONCE_SIZE;

// ServerHello trailing extension: version | server_nonce | confirm_mac
static constexpr size_t RESUME_ACCEPT_SIZE = 1 + RESUME_NONCE_SIZE + RESUME_MAC_SIZE;  // 33

struct ResumeTicket {
  uint8_t session_id[RESUME_SESSION_ID_SIZE];
  uint8_t secret[RESUME_SECRET_SIZE];
};

/// Fixed-slot RAM cache of single-use resume tickets. Lost on reboot by
/// design: clients fall back to a full handshake.
class ResumeTicketCache {
 public:
  /// Generate a fresh ticket into out and store it, evicting the oldest
  /// slot. Returns false (and stores nothing) if the RNG fails.
  bool issue(ResumeTicket &out);
  /// Accept a resume offer: verify and consume the ticket (single use; a
  /// forged MAC never burns one), build both transport ciphers, and fill
  /// the RESUME_ACCEPT_SIZE ServerHello extension. Returns false with
  /// nothing allocated on any miss or failure. Secrets are wiped internally.
  bool try_accept(const uint8_t *offer, size_t offer_len, const uint8_t *prologue, size_t prologue_len,
                  uint8_t *out_ext, NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher);
  /// Forget every ticket (PSK change).
  void clear();

 protected:
  bool take_verified_(const uint8_t *offer, uint8_t *secret_out);

  static constexpr uint8_t SLOTS = 4;
  ResumeTicket slots_[SLOTS];
  bool used_[SLOTS]{};
  uint8_t next_{0};
};

/// offer_mac for the ClientHello resume offer (what a client computes and
/// try_accept checks).
bool resume_compute_offer_mac(const uint8_t *secret, const uint8_t *session_id, const uint8_t *client_nonce,
                              uint8_t *out_mac);

/// confirm_mac for the ServerHello extension.
bool resume_compute_confirm_mac(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                                uint8_t *out_mac);

/// Derive the transport keys. k_c2d encrypts client-to-device traffic,
/// k_d2c device-to-client.
bool resume_derive_keys(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                        const uint8_t *prologue, size_t prologue_len, uint8_t *k_c2d, uint8_t *k_d2c);

/// Build a ChaChaPoly cipher state keyed with key (32 bytes); nullptr on
/// failure. Nonce counter starts at 0, exactly like a post-split cipher.
NoiseCipherState *resume_make_cipher(const uint8_t *key);

}  // namespace esphome::noise
#endif  // USE_NOISE
