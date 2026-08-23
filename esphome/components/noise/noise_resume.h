#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NOISE
#include <cstddef>
#include <cstdint>

#include <noise/protocol.h>

namespace esphome::noise {

/** Session resume for the noise transports.
 *
 * After a full NNpsk0 handshake the responder issues a single-use ticket
 * (session id + secret) over the encrypted channel. A client holding a
 * ticket places a resume offer in its ClientHello; the responder proves
 * possession of the secret in its ServerHello and both sides derive the
 * transport keys with HKDF-SHA256 alone, skipping the two curve25519
 * operations of a full handshake (~37 ms on ESP32, ~290 ms on ESP8266 at
 * 80 MHz). Old peers ignore the extension bytes on both sides, so every
 * mismatch degrades to a normal full handshake on the same connection.
 *
 * All HKDF calls use the Noise construction (noise_hashstate_hkdf):
 * temp = HMAC-SHA256(key, data); out1 = HMAC(temp, 0x01);
 * out2 = HMAC(temp, out1 || 0x02).
 *
 *   offer_mac   = HKDF(secret, "offer"   || session_id || client_nonce).out1[:16]
 *   confirm_mac = HKDF(secret, "confirm" || client_nonce || server_nonce).out1[:16]
 *   k_c2d, k_d2c = HKDF(secret, "keys"   || client_nonce || server_nonce
 *                                         || SHA256(prologue))   (32 bytes each)
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
  bool valid{false};
};

/// Fixed-slot RAM cache of single-use resume tickets. Lost on reboot by
/// design: clients fall back to a full handshake.
class ResumeTicketCache {
 public:
  /// Generate and store a fresh ticket, evicting the oldest slot.
  /// Returns false (and stores nothing) if the RNG fails.
  bool issue(ResumeTicket &out);
  /// Verify a wire offer (RESUME_OFFER_SIZE bytes, version already checked).
  /// On a valid MAC the ticket is consumed (single use) and its secret is
  /// copied to secret_out. A miss or a bad MAC leaves the cache unchanged so
  /// an attacker cannot burn tickets.
  bool take_verified(const uint8_t *offer, uint8_t *secret_out);
  /// Forget every ticket (PSK change).
  void clear();

 protected:
  static constexpr uint8_t SLOTS = 4;
  ResumeTicket slots_[SLOTS];
  uint8_t next_{0};
};

/// Best-effort secure wipe (not optimized away).
void resume_wipe(void *p, size_t len);

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
