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
 * error code, 0 on success.
 *
 * Methods are deliberately small separate functions so callers on tight
 * stacks (RP2040 core0 scratch bank) never pay for more than one branch;
 * the curve25519 step alone needs ~2KB of stack.
 */
class NoiseResponderHandshake {
 public:
  enum class Action : uint8_t { READ, WRITE, SPLIT, FAILED };

  ~NoiseResponderHandshake();

  /// Create and start the handshake with the given PSK and prologue.
  int init(const psk_t &psk, const uint8_t *prologue, size_t prologue_len);
  Action action() const;
  /// Process one received handshake message.
  int read_message(const uint8_t *data, size_t len);
  /// Produce the next handshake message into out (out_len receives its size).
  int write_message(uint8_t *out, size_t capacity, size_t &out_len);
  /// Hand out the transport ciphers and free the handshake state. The caller
  /// owns both cipher states and must free them with noise_cipherstate_free().
  int split(NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher);

 protected:
  NoiseHandshakeState *handshake_{nullptr};
};

}  // namespace esphome::noise
#endif  // USE_NOISE
