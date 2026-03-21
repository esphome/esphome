#include "esphome/core/defines.h"
#ifdef USE_API_NOISE

#include <benchmark/benchmark.h>
#include <cstring>

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

// --- Encrypt a typical sensor state message (small payload ~14 bytes) ---
// This is the most common message encrypted on every sensor update.

static void NoiseEncrypt_SmallMessage(benchmark::State &state) {
  NoiseCipherState *cipher = create_cipher();
  if (cipher == nullptr) {
    state.SkipWithError("Failed to create cipher state");
    return;
  }

  size_t mac_len = noise_cipherstate_get_mac_length(cipher);
  // Typical SensorStateResponse: 4 bytes type+len header + ~10 bytes payload
  constexpr size_t plaintext_size = 14;
  size_t buf_capacity = plaintext_size + mac_len;
  auto buffer = std::make_unique<uint8_t[]>(buf_capacity);
  memset(buffer.get(), 0x42, plaintext_size);

  uint64_t nonce = 0;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      noise_cipherstate_set_nonce(cipher, nonce++);
      NoiseBuffer mbuf;
      noise_buffer_set_inout(mbuf, buffer.get(), plaintext_size, buf_capacity);
      noise_cipherstate_encrypt(cipher, &mbuf);
    }
    benchmark::DoNotOptimize(buffer[0]);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  noise_cipherstate_free(cipher);
}
BENCHMARK(NoiseEncrypt_SmallMessage);

// --- Encrypt a medium message (~128 bytes, typical for LightStateResponse) ---

static void NoiseEncrypt_MediumMessage(benchmark::State &state) {
  NoiseCipherState *cipher = create_cipher();
  if (cipher == nullptr) {
    state.SkipWithError("Failed to create cipher state");
    return;
  }

  size_t mac_len = noise_cipherstate_get_mac_length(cipher);
  constexpr size_t plaintext_size = 128;
  size_t buf_capacity = plaintext_size + mac_len;
  auto buffer = std::make_unique<uint8_t[]>(buf_capacity);
  memset(buffer.get(), 0x42, plaintext_size);

  uint64_t nonce = 0;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      noise_cipherstate_set_nonce(cipher, nonce++);
      NoiseBuffer mbuf;
      noise_buffer_set_inout(mbuf, buffer.get(), plaintext_size, buf_capacity);
      noise_cipherstate_encrypt(cipher, &mbuf);
    }
    benchmark::DoNotOptimize(buffer[0]);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  noise_cipherstate_free(cipher);
}
BENCHMARK(NoiseEncrypt_MediumMessage);

// --- Encrypt a large message (~1024 bytes, typical for DeviceInfoResponse) ---

static void NoiseEncrypt_LargeMessage(benchmark::State &state) {
  NoiseCipherState *cipher = create_cipher();
  if (cipher == nullptr) {
    state.SkipWithError("Failed to create cipher state");
    return;
  }

  size_t mac_len = noise_cipherstate_get_mac_length(cipher);
  constexpr size_t plaintext_size = 1024;
  size_t buf_capacity = plaintext_size + mac_len;
  auto buffer = std::make_unique<uint8_t[]>(buf_capacity);
  memset(buffer.get(), 0x42, plaintext_size);

  uint64_t nonce = 0;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      noise_cipherstate_set_nonce(cipher, nonce++);
      NoiseBuffer mbuf;
      noise_buffer_set_inout(mbuf, buffer.get(), plaintext_size, buf_capacity);
      noise_cipherstate_encrypt(cipher, &mbuf);
    }
    benchmark::DoNotOptimize(buffer[0]);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  noise_cipherstate_free(cipher);
}
BENCHMARK(NoiseEncrypt_LargeMessage);

}  // namespace esphome::api::benchmarks

#endif  // USE_API_NOISE
