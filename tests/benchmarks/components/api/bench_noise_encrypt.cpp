#include "esphome/core/defines.h"
#ifdef USE_API_NOISE

#include <benchmark/benchmark.h>
#include <cstring>
#include <memory>

#include "noise/protocol.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to create and initialize a NoiseCipherState with ChaChaPoly.
// Returns nullptr on failure.
static NoiseCipherState *create_cipher() {
  NoiseCipherState *cipher = nullptr;
  int err = noise_cipherstate_new_by_id(&cipher, NOISE_CIPHER_CHACHAPOLY);
  if (err != NOISE_ERROR_NONE || cipher == nullptr)
    return nullptr;

  // Initialize with a dummy 32-byte key (same pattern as handshake split produces)
  uint8_t key[32];
  memset(key, 0xAB, sizeof(key));
  err = noise_cipherstate_init_key(cipher, key, sizeof(key));
  if (err != NOISE_ERROR_NONE) {
    noise_cipherstate_free(cipher);
    return nullptr;
  }
  return cipher;
}

// Benchmark helper matching the exact pattern from
// APINoiseFrameHelper::write_protobuf_messages:
//   - noise_buffer_init + noise_buffer_set_inout (same as production)
//   - No explicit set_nonce (production relies on internal nonce increment)
//   - Error checking on encrypt return
static void noise_encrypt_bench(benchmark::State &state, size_t plaintext_size) {
  NoiseCipherState *cipher = create_cipher();
  if (cipher == nullptr) {
    state.SkipWithError("Failed to create cipher state");
    return;
  }

  size_t mac_len = noise_cipherstate_get_mac_length(cipher);
  size_t buf_capacity = plaintext_size + mac_len;
  auto buffer = std::make_unique<uint8_t[]>(buf_capacity);
  memset(buffer.get(), 0x42, plaintext_size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      // Match production: init buffer, set inout, encrypt
      NoiseBuffer mbuf;
      noise_buffer_init(mbuf);
      noise_buffer_set_inout(mbuf, buffer.get(), plaintext_size, buf_capacity);

      int err = noise_cipherstate_encrypt(cipher, &mbuf);
      if (err != NOISE_ERROR_NONE) {
        state.SkipWithError("noise_cipherstate_encrypt failed");
        noise_cipherstate_free(cipher);
        return;
      }
    }
    benchmark::DoNotOptimize(buffer[0]);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  noise_cipherstate_free(cipher);
}

// --- Encrypt a typical sensor state message (small payload ~14 bytes) ---
// This is the most common message encrypted on every sensor update.
// 4 bytes type+len header + ~10 bytes payload.

static void NoiseEncrypt_SmallMessage(benchmark::State &state) { noise_encrypt_bench(state, 14); }
BENCHMARK(NoiseEncrypt_SmallMessage);

// --- Encrypt a medium message (~128 bytes, typical for LightStateResponse) ---

static void NoiseEncrypt_MediumMessage(benchmark::State &state) { noise_encrypt_bench(state, 128); }
BENCHMARK(NoiseEncrypt_MediumMessage);

// --- Encrypt a large message (~1024 bytes, typical for DeviceInfoResponse) ---

static void NoiseEncrypt_LargeMessage(benchmark::State &state) { noise_encrypt_bench(state, 1024); }
BENCHMARK(NoiseEncrypt_LargeMessage);

// Benchmark helper matching the exact pattern from
// APINoiseFrameHelper::read_packet:
//   - noise_buffer_init + noise_buffer_set_inout with capacity == size (decrypt shrinks)
//   - Error checking on decrypt return
//
// Pre-encrypts kInnerIterations messages with sequential nonces before the
// timed loop. Each outer iteration re-inits the decrypt key to reset the
// nonce back to 0, then decrypts all pre-encrypted messages in sequence.
// The init_key cost is amortized over kInnerIterations decrypts.
static void noise_decrypt_bench(benchmark::State &state, size_t plaintext_size) {
  NoiseCipherState *encrypt_cipher = create_cipher();
  NoiseCipherState *decrypt_cipher = create_cipher();
  if (encrypt_cipher == nullptr || decrypt_cipher == nullptr) {
    state.SkipWithError("Failed to create cipher state");
    if (encrypt_cipher)
      noise_cipherstate_free(encrypt_cipher);
    if (decrypt_cipher)
      noise_cipherstate_free(decrypt_cipher);
    return;
  }

  size_t mac_len = noise_cipherstate_get_mac_length(encrypt_cipher);
  size_t encrypted_size = plaintext_size + mac_len;

  // Pre-encrypt kInnerIterations messages with sequential nonces (0..N-1).
  auto ciphertexts = std::make_unique<uint8_t[]>(encrypted_size * kInnerIterations);
  for (int i = 0; i < kInnerIterations; i++) {
    uint8_t *ct = ciphertexts.get() + i * encrypted_size;
    memset(ct, 0x42, plaintext_size);
    NoiseBuffer enc_buf;
    noise_buffer_init(enc_buf);
    noise_buffer_set_inout(enc_buf, ct, plaintext_size, encrypted_size);
    int err = noise_cipherstate_encrypt(encrypt_cipher, &enc_buf);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Pre-encrypt failed");
      noise_cipherstate_free(encrypt_cipher);
      noise_cipherstate_free(decrypt_cipher);
      return;
    }
  }

  // Working buffer — decrypt modifies in place
  auto buffer = std::make_unique<uint8_t[]>(encrypted_size);
  static constexpr uint8_t KEY[32] = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                      0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                      0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB};

  for (auto _ : state) {
    // Reset nonce to 0 by re-initing the key (amortized over kInnerIterations)
    noise_cipherstate_init_key(decrypt_cipher, KEY, sizeof(KEY));

    for (int i = 0; i < kInnerIterations; i++) {
      // Copy ciphertext into working buffer (decrypt modifies in place)
      memcpy(buffer.get(), ciphertexts.get() + i * encrypted_size, encrypted_size);

      // Decrypt matching production pattern
      NoiseBuffer mbuf;
      noise_buffer_init(mbuf);
      noise_buffer_set_inout(mbuf, buffer.get(), encrypted_size, encrypted_size);

      int err = noise_cipherstate_decrypt(decrypt_cipher, &mbuf);
      if (err != NOISE_ERROR_NONE) {
        state.SkipWithError("noise_cipherstate_decrypt failed");
        noise_cipherstate_free(encrypt_cipher);
        noise_cipherstate_free(decrypt_cipher);
        return;
      }
    }
    benchmark::DoNotOptimize(buffer[0]);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  noise_cipherstate_free(encrypt_cipher);
  noise_cipherstate_free(decrypt_cipher);
}

// --- Decrypt benchmarks (matching read_packet path) ---

static void NoiseDecrypt_SmallMessage(benchmark::State &state) { noise_decrypt_bench(state, 14); }
BENCHMARK(NoiseDecrypt_SmallMessage);

static void NoiseDecrypt_MediumMessage(benchmark::State &state) { noise_decrypt_bench(state, 128); }
BENCHMARK(NoiseDecrypt_MediumMessage);

static void NoiseDecrypt_LargeMessage(benchmark::State &state) { noise_decrypt_bench(state, 1024); }
BENCHMARK(NoiseDecrypt_LargeMessage);

// --- Full Noise_NNpsk0 handshake benchmark ---
// Measures the complete handshake between initiator and responder:
//   - Create handshake states for both sides
//   - Set PSK and prologue
//   - Exchange messages (initiator write -> responder read -> responder write -> initiator read)
//   - Split to get cipher states
// This is dominated by Curve25519 DH operations (expensive on ESP8266).
// No inner iterations — each handshake is already expensive enough.

static void NoiseHandshake_Full(benchmark::State &state) {
  // Matching ESPHome's protocol: Noise_NNpsk0_25519_ChaChaPoly_SHA256
  NoiseProtocolId nid;
  memset(&nid, 0, sizeof(nid));
  nid.pattern_id = NOISE_PATTERN_NN;
  nid.cipher_id = NOISE_CIPHER_CHACHAPOLY;
  nid.dh_id = NOISE_DH_CURVE25519;
  nid.prefix_id = NOISE_PREFIX_STANDARD;
  nid.hybrid_id = NOISE_DH_NONE;
  nid.hash_id = NOISE_HASH_SHA256;
  nid.modifier_ids[0] = NOISE_MODIFIER_PSK0;

  // Dummy PSK (32 bytes) and prologue matching production setup
  static constexpr uint8_t PSK[32] = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                      0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                      0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB};
  static constexpr uint8_t PROLOGUE[] = "NoESPHome";

  // Message buffer for handshake exchange (max handshake message ~96 bytes)
  uint8_t msg_buf[128];

  for (auto _ : state) {
    NoiseHandshakeState *initiator = nullptr;
    NoiseHandshakeState *responder = nullptr;
    NoiseCipherState *init_send = nullptr, *init_recv = nullptr;
    NoiseCipherState *resp_send = nullptr, *resp_recv = nullptr;
    int err;

    // Create both handshake states
    err = noise_handshakestate_new_by_id(&initiator, &nid, NOISE_ROLE_INITIATOR);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Failed to create initiator");
      return;
    }
    err = noise_handshakestate_new_by_id(&responder, &nid, NOISE_ROLE_RESPONDER);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Failed to create responder");
      noise_handshakestate_free(initiator);
      return;
    }

    // Set PSK and prologue on both sides
    noise_handshakestate_set_pre_shared_key(initiator, PSK, sizeof(PSK));
    noise_handshakestate_set_pre_shared_key(responder, PSK, sizeof(PSK));
    noise_handshakestate_set_prologue(initiator, PROLOGUE, sizeof(PROLOGUE) - 1);
    noise_handshakestate_set_prologue(responder, PROLOGUE, sizeof(PROLOGUE) - 1);

    noise_handshakestate_start(initiator);
    noise_handshakestate_start(responder);

    // Message 1: Initiator -> Responder
    NoiseBuffer write_buf, read_buf;
    noise_buffer_set_output(write_buf, msg_buf, sizeof(msg_buf));
    err = noise_handshakestate_write_message(initiator, &write_buf, nullptr);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Initiator write_message failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      return;
    }

    noise_buffer_set_input(read_buf, msg_buf, write_buf.size);
    err = noise_handshakestate_read_message(responder, &read_buf, nullptr);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Responder read_message failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      return;
    }

    // Message 2: Responder -> Initiator
    noise_buffer_set_output(write_buf, msg_buf, sizeof(msg_buf));
    err = noise_handshakestate_write_message(responder, &write_buf, nullptr);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Responder write_message failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      return;
    }

    noise_buffer_set_input(read_buf, msg_buf, write_buf.size);
    err = noise_handshakestate_read_message(initiator, &read_buf, nullptr);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Initiator read_message failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      return;
    }

    // Split to get cipher states
    err = noise_handshakestate_split(initiator, &init_send, &init_recv);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Initiator split failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      return;
    }
    err = noise_handshakestate_split(responder, &resp_send, &resp_recv);
    if (err != NOISE_ERROR_NONE) {
      state.SkipWithError("Responder split failed");
      noise_handshakestate_free(initiator);
      noise_handshakestate_free(responder);
      noise_cipherstate_free(init_send);
      noise_cipherstate_free(init_recv);
      return;
    }

    benchmark::DoNotOptimize(init_send);

    // Cleanup
    noise_handshakestate_free(initiator);
    noise_handshakestate_free(responder);
    noise_cipherstate_free(init_send);
    noise_cipherstate_free(init_recv);
    noise_cipherstate_free(resp_send);
    noise_cipherstate_free(resp_recv);
  }
}
BENCHMARK(NoiseHandshake_Full);

}  // namespace esphome::api::benchmarks

#endif  // USE_API_NOISE
