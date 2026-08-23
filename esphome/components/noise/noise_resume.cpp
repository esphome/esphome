#include "noise_resume.h"
#ifdef USE_NOISE
#include <cstring>

#include <noise/protocol.h>

#include "esphome/core/helpers.h"

namespace esphome::noise {

/// Noise-construction HKDF-SHA256; out2 may be scratch the caller wipes.
static bool resume_hkdf_(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *out1,
                         size_t out1_len, uint8_t *out2, size_t out2_len) {
  NoiseHashState *hash = nullptr;
  if (noise_hashstate_new_by_id(&hash, NOISE_HASH_SHA256) != NOISE_ERROR_NONE) {
    return false;
  }
  int err = noise_hashstate_hkdf(hash, key, key_len, data, data_len, out1, out1_len, out2, out2_len);
  noise_hashstate_free(hash);
  return err == NOISE_ERROR_NONE;
}

template<size_t N>
static bool resume_mac_(const uint8_t *secret, const char (&label)[N], const uint8_t *a, size_t a_len, const uint8_t *b,
                        size_t b_len, uint8_t *out_mac) {
  constexpr size_t label_len = N - 1;  // drop the terminating NUL
  // label || a || b, largest use is "confirm"(7) + 16 + 16 = 39
  uint8_t data[7 + RESUME_NONCE_SIZE + RESUME_NONCE_SIZE];
  uint8_t scratch[32];
  std::memcpy(data, label, label_len);
  std::memcpy(data + label_len, a, a_len);
  std::memcpy(data + label_len + a_len, b, b_len);
  bool ok = resume_hkdf_(secret, RESUME_SECRET_SIZE, data, label_len + a_len + b_len, out_mac, RESUME_MAC_SIZE, scratch,
                         sizeof(scratch));
  noise_clean(scratch, sizeof(scratch));
  return ok;
}

bool ResumeTicketCache::issue(ResumeTicket &out) {
  if (!random_bytes(out.session_id, RESUME_SESSION_ID_SIZE) || !random_bytes(out.secret, RESUME_SECRET_SIZE)) {
    return false;
  }
  uint8_t slot = this->next_;
  this->next_ = static_cast<uint8_t>((slot + 1) % SLOTS);
  this->slots_[slot] = out;
  this->used_[slot] = true;
  return true;
}

bool ResumeTicketCache::take_verified_(const uint8_t *offer, uint8_t *secret_out) {
  const uint8_t *session_id = offer + RESUME_OFFER_SESSION_ID_OFFSET;
  const uint8_t *client_nonce = offer + RESUME_OFFER_NONCE_OFFSET;
  const uint8_t *offer_mac = offer + RESUME_OFFER_MAC_OFFSET;
  for (uint8_t i = 0; i < SLOTS; i++) {
    ResumeTicket &slot = this->slots_[i];
    if (!this->used_[i] || std::memcmp(slot.session_id, session_id, RESUME_SESSION_ID_SIZE) != 0) {
      continue;
    }
    uint8_t expected[RESUME_MAC_SIZE];
    bool ok = resume_compute_offer_mac(slot.secret, session_id, client_nonce, expected) &&
              noise_is_equal(expected, offer_mac, RESUME_MAC_SIZE);
    noise_clean(expected, sizeof(expected));
    if (!ok) {
      // Bad MAC: leave the ticket so a forger cannot burn it
      return false;
    }
    std::memcpy(secret_out, slot.secret, RESUME_SECRET_SIZE);
    noise_clean(&slot, sizeof(slot));
    this->used_[i] = false;
    return true;
  }
  return false;
}

bool ResumeTicketCache::try_accept(const uint8_t *offer, size_t offer_len, const uint8_t *prologue, size_t prologue_len,
                                   uint8_t *out_ext, NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher) {
  if (offer_len != RESUME_OFFER_SIZE || offer[0] != RESUME_OFFER_VERSION) {
    return false;
  }
  uint8_t secret[RESUME_SECRET_SIZE];
  if (!this->take_verified_(offer, secret)) {
    return false;
  }
  // From here every failure still falls back to the full handshake; the
  // ticket is spent, which is harmless (the client gets a fresh one).
  const uint8_t *client_nonce = offer + RESUME_OFFER_NONCE_OFFSET;
  uint8_t *server_nonce = out_ext + 1;
  uint8_t *confirm_mac = out_ext + 1 + RESUME_NONCE_SIZE;
  uint8_t k_c2d[32];
  uint8_t k_d2c[32];
  out_ext[0] = RESUME_ACCEPT_VERSION;
  bool ok = random_bytes(server_nonce, RESUME_NONCE_SIZE) &&
            resume_compute_confirm_mac(secret, client_nonce, server_nonce, confirm_mac) &&
            resume_derive_keys(secret, client_nonce, server_nonce, prologue, prologue_len, k_c2d, k_d2c);
  noise_clean(secret, sizeof(secret));
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
  return ok;
}

void ResumeTicketCache::clear() {
  noise_clean(this->slots_, sizeof(this->slots_));
  std::memset(this->used_, 0, sizeof(this->used_));
}

bool resume_compute_offer_mac(const uint8_t *secret, const uint8_t *session_id, const uint8_t *client_nonce,
                              uint8_t *out_mac) {
  return resume_mac_(secret, "offer", session_id, RESUME_SESSION_ID_SIZE, client_nonce, RESUME_NONCE_SIZE, out_mac);
}

bool resume_compute_confirm_mac(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                                uint8_t *out_mac) {
  return resume_mac_(secret, "confirm", client_nonce, RESUME_NONCE_SIZE, server_nonce, RESUME_NONCE_SIZE, out_mac);
}

bool resume_derive_keys(const uint8_t *secret, const uint8_t *client_nonce, const uint8_t *server_nonce,
                        const uint8_t *prologue, size_t prologue_len, uint8_t *k_c2d, uint8_t *k_d2c) {
  NoiseHashState *hash = nullptr;
  if (noise_hashstate_new_by_id(&hash, NOISE_HASH_SHA256) != NOISE_ERROR_NONE) {
    return false;
  }
  // "keys"(4) || client_nonce(16) || server_nonce(16) || SHA256(prologue)(32)
  uint8_t data[4 + RESUME_NONCE_SIZE + RESUME_NONCE_SIZE + 32];
  std::memcpy(data, "keys", 4);
  std::memcpy(data + 4, client_nonce, RESUME_NONCE_SIZE);
  std::memcpy(data + 4 + RESUME_NONCE_SIZE, server_nonce, RESUME_NONCE_SIZE);
  int err = noise_hashstate_hash_one(hash, prologue, prologue_len, data + 4 + 2 * RESUME_NONCE_SIZE, 32);
  if (err == NOISE_ERROR_NONE) {
    err = noise_hashstate_hkdf(hash, secret, RESUME_SECRET_SIZE, data, sizeof(data), k_c2d, 32, k_d2c, 32);
  }
  noise_hashstate_free(hash);
  noise_clean(data, sizeof(data));
  return err == NOISE_ERROR_NONE;
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
