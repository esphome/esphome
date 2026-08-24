#include "noise_resume.h"
#ifdef USE_NOISE
#include <cstring>

#include <noise/protocol.h>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::noise {

static const char LABEL_OFFER[] PROGMEM = "offer";
static const char LABEL_CONFIRM[] PROGMEM = "confirm";
static const char LABEL_KEYS[] PROGMEM = "keys";
// Largest KDF input: "keys" || client_nonce || server_nonce || SHA256(prologue)
static constexpr size_t RESUME_KDF_MAX_DATA = sizeof(LABEL_KEYS) - 1 + RESUME_NONCE_SIZE + RESUME_NONCE_SIZE + 32;
static_assert(sizeof(LABEL_CONFIRM) - 1 + RESUME_NONCE_SIZE + RESUME_NONCE_SIZE <= RESUME_KDF_MAX_DATA,
              "MAC input must fit the KDF buffer");

/// Noise-construction HKDF-SHA256 keyed with the ticket secret over
/// label || a || b [|| SHA256(hash_in)]. out2 == nullptr means MAC only.
static bool resume_kdf(const uint8_t *secret, const char *label, size_t label_len, const uint8_t *a, size_t a_len,
                       const uint8_t *b, size_t b_len, const uint8_t *hash_in, size_t hash_in_len, uint8_t *out1,
                       size_t out1_len, uint8_t *out2) {
  uint8_t data[RESUME_KDF_MAX_DATA];
  uint8_t scratch[32];
  size_t len = label_len + a_len + b_len;
  progmem_memcpy(data, label, label_len);
  std::memcpy(data + label_len, a, a_len);
  std::memcpy(data + label_len + a_len, b, b_len);
  NoiseHashState *hash = nullptr;
  if (noise_hashstate_new_by_id(&hash, NOISE_HASH_SHA256) != NOISE_ERROR_NONE) {
    return false;
  }
  int err = NOISE_ERROR_NONE;
  if (hash_in != nullptr) {
    err = noise_hashstate_hash_one(hash, hash_in, hash_in_len, data + len, 32);
    len += 32;
  }
  if (err == NOISE_ERROR_NONE) {
    err = noise_hashstate_hkdf(hash, secret, RESUME_SECRET_SIZE, data, len, out1, out1_len,
                               out2 != nullptr ? out2 : scratch, 32);
  }
  noise_hashstate_free(hash);
  noise_clean(data, sizeof(data));
  noise_clean(scratch, sizeof(scratch));
  return err == NOISE_ERROR_NONE;
}

bool ResumeTicketCache::issue(ResumeTicket &out) {
  if (!random_bytes(reinterpret_cast<uint8_t *>(&out), sizeof(out))) {
    return false;
  }
  uint8_t slot = this->next_;
  this->next_ = static_cast<uint8_t>((slot + 1) % SLOTS);
  this->slots_[slot] = out;
  this->used_mask_ |= static_cast<uint8_t>(1u << slot);
  return true;
}

size_t ResumeTicketCache::try_accept(const uint8_t *offer, size_t offer_len, const uint8_t *prologue,
                                     size_t prologue_len, uint8_t *out_ext, size_t out_capacity,
                                     NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher) {
  if (offer_len != RESUME_OFFER_SIZE || offer[0] != RESUME_OFFER_VERSION || out_capacity < RESUME_ACCEPT_SIZE) {
    return 0;
  }
  const uint8_t *session_id = offer + RESUME_OFFER_SESSION_ID_OFFSET;
  const uint8_t *client_nonce = offer + RESUME_OFFER_NONCE_OFFSET;
  ResumeTicket *ticket = nullptr;
  for (uint8_t i = 0; i < SLOTS; i++) {
    if ((this->used_mask_ & (1u << i)) &&
        std::memcmp(this->slots_[i].session_id, session_id, RESUME_SESSION_ID_SIZE) == 0) {
      ticket = &this->slots_[i];
      this->used_mask_ &= static_cast<uint8_t>(~(1u << i));
      break;
    }
  }
  if (ticket == nullptr) {
    return 0;
  }
  uint8_t expected[RESUME_MAC_SIZE];
  bool ok = resume_compute_offer_mac(ticket->secret, session_id, client_nonce, expected) &&
            noise_is_equal(expected, offer + RESUME_OFFER_MAC_OFFSET, RESUME_MAC_SIZE);
  noise_clean(expected, sizeof(expected));
  if (!ok) {
    // Bad MAC: keep the ticket so a forger cannot burn it
    this->used_mask_ |= static_cast<uint8_t>(1u << static_cast<uint8_t>(ticket - this->slots_));
    return 0;
  }
  // The ticket is spent from here; any later failure falls back to the full
  // handshake and the client gets a fresh one.
  uint8_t *server_nonce = out_ext + 1;
  uint8_t k_c2d[32];
  uint8_t k_d2c[32];
  out_ext[0] = RESUME_ACCEPT_VERSION;
  ok = random_bytes(server_nonce, RESUME_NONCE_SIZE) &&
       resume_compute_confirm_mac(ticket->secret, client_nonce, server_nonce, out_ext + 1 + RESUME_NONCE_SIZE) &&
       resume_derive_keys(ticket->secret, client_nonce, server_nonce, prologue, prologue_len, k_c2d, k_d2c);
  noise_clean(ticket, sizeof(*ticket));
  if (ok) {
    recv_cipher = resume_make_cipher(k_c2d);
    send_cipher = resume_make_cipher(k_d2c);
    ok = recv_cipher != nullptr && send_cipher != nullptr;
    if (!ok) {
      noise_cipherstate_free(recv_cipher);
      noise_cipherstate_free(send_cipher);
      recv_cipher = nullptr;
      send_cipher = nullptr;
    }
  }
  noise_clean(k_c2d, sizeof(k_c2d));
  noise_clean(k_d2c, sizeof(k_d2c));
  return ok ? RESUME_ACCEPT_SIZE : 0;
}

void ResumeTicketCache::clear() {
  noise_clean(this->slots_, sizeof(this->slots_));
  this->used_mask_ = 0;
}

bool resume_compute_offer_mac(const uint8_t *secret, const uint8_t *session_id, const uint8_t *client_nonce,
                              uint8_t *out_mac) {
  return resume_kdf(secret, LABEL_OFFER, sizeof(LABEL_OFFER) - 1, session_id, RESUME_SESSION_ID_SIZE, client_nonce,
                    RESUME_NONCE_SIZE, nullptr, 0, out_mac, RESUME_MAC_SIZE, nullptr);
}

bool resume_compute_confirm_mac(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                                uint8_t *out_mac) {
  return resume_kdf(secret, LABEL_CONFIRM, sizeof(LABEL_CONFIRM) - 1, client_nonce, RESUME_NONCE_SIZE, server_nonce,
                    RESUME_NONCE_SIZE, nullptr, 0, out_mac, RESUME_MAC_SIZE, nullptr);
}

bool resume_derive_keys(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                        const uint8_t *prologue, size_t prologue_len, uint8_t *k_c2d, uint8_t *k_d2c) {
  return resume_kdf(secret, LABEL_KEYS, sizeof(LABEL_KEYS) - 1, client_nonce, RESUME_NONCE_SIZE, server_nonce,
                    RESUME_NONCE_SIZE, prologue, prologue_len, k_c2d, 32, k_d2c);
}

NoiseCipherState *resume_make_cipher(const uint8_t *key) {
  NoiseCipherState *cipher = nullptr;
  if (noise_cipherstate_new_by_id(&cipher, NOISE_CIPHER_CHACHAPOLY) != NOISE_ERROR_NONE) {
    return nullptr;
  }
  if (noise_cipherstate_init_key(cipher, key, 32) != NOISE_ERROR_NONE) {
    noise_cipherstate_free(cipher);
    return nullptr;
  }
  return cipher;
}

}  // namespace esphome::noise
#endif  // USE_NOISE
