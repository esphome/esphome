#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NOISE
#include <cstddef>
#include <cstdint>

#include <noise/protocol.h>

#include "noise.h"

namespace esphome::noise {

/** Sans-IO responder side of a Noise_NNpsk0_25519_ChaChaPoly_SHA256 handshake.
 *
 * Owns only the noise-c handshake state; the caller moves the raw handshake
 * messages (no framing) over its own transport, driven by action():
 * read_message() while READ, write_message() while WRITE, then split() to
 * take ownership of the transport ciphers. All methods return a noise-c
 * error code, 0 on success. Called outside their action() step (before
 * init(), after split()) the message methods return a noise-c error rather
 * than crashing; the library checks its state argument.
 *
 * Methods are deliberately small separate functions so callers on tight
 * stacks (RP2040 core0 scratch bank) never pay for more than one branch;
 * the curve25519 step alone needs ~2KB of stack.
 */
class NoiseResponderHandshake {
 public:
  // The ACTION_ prefix is macro-collision safety: SDK headers #define bare
  // names like READ/WRITE, and macros expand even inside an enum class.
  enum class Action : uint8_t { ACTION_READ, ACTION_WRITE, ACTION_SPLIT, ACTION_FAILED };

  NoiseResponderHandshake() = default;
  ~NoiseResponderHandshake();
  // Owns a raw noise-c handshake state; copying would double free it
  NoiseResponderHandshake(const NoiseResponderHandshake &) = delete;
  NoiseResponderHandshake &operator=(const NoiseResponderHandshake &) = delete;

  /// Create and start the handshake with the given PSK and prologue. A
  /// repeated call frees the previous handshake state and starts over.
  [[nodiscard]] int init(const psk_t &psk, const uint8_t *prologue, size_t prologue_len);
  /// ACTION_FAILED is the catch-all: returned before init(), after split()
  /// has released the state, and when noise-c reports a failed handshake.
  [[nodiscard]] Action action() const;
  /// Process one received handshake message. The buffer is consumed in
  /// place: noise-c decrypts into it and zeroes it before returning.
  [[nodiscard]] int read_message(uint8_t *data, size_t len);
  /// Produce the next handshake message into out; out_len receives its size
  /// and is zero on error.
  [[nodiscard]] int write_message(uint8_t *out, size_t capacity, size_t &out_len);
  /// Hand out the transport ciphers and free the handshake state. The caller
  /// owns both cipher states and must free them with noise_cipherstate_free();
  /// both are set to nullptr on error.
  [[nodiscard]] int split(NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher);

 protected:
  int fail_init_(int err);

  NoiseHandshakeState *handshake_{nullptr};
};

}  // namespace esphome::noise
#endif  // USE_NOISE
