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

}  // namespace esphome::api::benchmarks

#endif  // USE_API_NOISE
