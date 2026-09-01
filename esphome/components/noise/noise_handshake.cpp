#include "noise_handshake.h"
#ifdef USE_NOISE
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::noise {

static const char *const TAG = "noise";

// Log the failing noise-c call at the same verbosity the api helper used
// before this class existed; callers only see one collapsed error code.
#define HANDSHAKE_STEP_LOG(func_name, err_code) \
  ESP_LOGVV(TAG, "%s failed: %s", LOG_STR_ARG(LOG_STR(func_name)), LOG_STR_ARG(noise_err_to_logstr(err_code)))

NoiseResponderHandshake::~NoiseResponderHandshake() {
  if (this->handshake_ != nullptr) {
    noise_handshakestate_free(this->handshake_);
    this->handshake_ = nullptr;
  }
}

int NoiseResponderHandshake::init(const psk_t &psk, const uint8_t *prologue, size_t prologue_len) {
  if (this->handshake_ != nullptr) {
    noise_handshakestate_free(this->handshake_);
    this->handshake_ = nullptr;
  }
  // Noise_NNpsk0_25519_ChaChaPoly_SHA256, built on the stack:
  // noise_handshakestate_new_by_id copies it, so a member would waste
  // 104 bytes per connection, and a static const would sit in RAM on
  // ESP8266 (.rodata is DRAM there).
  const NoiseProtocolId nid = {
      .prefix_id = NOISE_PREFIX_STANDARD,
      .pattern_id = NOISE_PATTERN_NN,
      .modifier_ids = {NOISE_MODIFIER_PSK0},
      .dh_id = NOISE_DH_CURVE25519,
      .cipher_id = NOISE_CIPHER_CHACHAPOLY,
      .hash_id = NOISE_HASH_SHA256,
      .hybrid_id = NOISE_DH_NONE,
  };

  int err = noise_handshakestate_new_by_id(&this->handshake_, &nid, NOISE_ROLE_RESPONDER);
  if (err != 0) {
    HANDSHAKE_STEP_LOG("noise_handshakestate_new_by_id", err);
    return err;
  }
  err = noise_handshakestate_set_pre_shared_key(this->handshake_, psk.data(), psk.size());
  if (err != 0) {
    HANDSHAKE_STEP_LOG("noise_handshakestate_set_pre_shared_key", err);
    return this->fail_init_(err);
  }
  err = noise_handshakestate_set_prologue(this->handshake_, prologue, prologue_len);
  if (err != 0) {
    HANDSHAKE_STEP_LOG("noise_handshakestate_set_prologue", err);
    return this->fail_init_(err);
  }
  err = noise_handshakestate_start(this->handshake_);
  if (err != 0) {
    HANDSHAKE_STEP_LOG("noise_handshakestate_start", err);
    return this->fail_init_(err);
  }
  return 0;
}

/// Release a half-initialized state so a failed init() leaves the object as
/// if init() was never called.
int NoiseResponderHandshake::fail_init_(int err) {
  noise_handshakestate_free(this->handshake_);
  this->handshake_ = nullptr;
  return err;
}

NoiseResponderHandshake::Action NoiseResponderHandshake::action() const {
  if (this->handshake_ == nullptr) {
    // A caller bug: init() was never called, or split() already released the state
    ESP_LOGVV(TAG, "action() on uninitialized or split handshake");
    return Action::ACTION_FAILED;
  }
  int raw = noise_handshakestate_get_action(this->handshake_);
  switch (raw) {
    case NOISE_ACTION_READ_MESSAGE:
      return Action::ACTION_READ;
    case NOISE_ACTION_WRITE_MESSAGE:
      return Action::ACTION_WRITE;
    case NOISE_ACTION_SPLIT:
      return Action::ACTION_SPLIT;
    default:
      // Preserve the raw code in debug logs; callers only see the collapsed enum
      ESP_LOGVV(TAG, "Unexpected noise action %d", raw);
      return Action::ACTION_FAILED;
  }
}

int NoiseResponderHandshake::read_message(uint8_t *data, size_t len) {
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_input(mbuf, data, len);
  return noise_handshakestate_read_message(this->handshake_, &mbuf, nullptr);
}

int NoiseResponderHandshake::write_message(uint8_t *out, size_t capacity, size_t &out_len) {
  out_len = 0;
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_output(mbuf, out, capacity);
  int err = noise_handshakestate_write_message(this->handshake_, &mbuf, nullptr);
  if (err == 0)
    out_len = mbuf.size;
  return err;
}

int NoiseResponderHandshake::split(NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher) {
  // Defined error postcondition: noise-c leaves the out-params unwritten on
  // its early error returns, so a caller passing uninitialized locals must
  // never see garbage to free
  send_cipher = nullptr;
  recv_cipher = nullptr;
  int err = noise_handshakestate_split(this->handshake_, &send_cipher, &recv_cipher);
  if (err != 0)
    return err;
  noise_handshakestate_free(this->handshake_);
  this->handshake_ = nullptr;
  return 0;
}

extern "C" {
// noise-c's only randomness source (the vendored library compiles no rand of
// its own); HWRNG backed. Lives in this TU so every handshake consumer links
// it and the definition can never be dropped from the archive.
void noise_rand_bytes(void *output, size_t len) {
  if (!esphome::random_bytes(reinterpret_cast<uint8_t *>(output), len)) {
    ESP_LOGE(TAG, "Acquiring random bytes failed; rebooting");
    arch_restart();
  }
}
}

}  // namespace esphome::noise
#endif  // USE_NOISE
