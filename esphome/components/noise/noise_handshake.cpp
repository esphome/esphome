#include "noise_handshake.h"
#ifdef USE_NOISE

namespace esphome::noise {

NoiseResponderHandshake::~NoiseResponderHandshake() {
  if (this->handshake_ != nullptr) {
    noise_handshakestate_free(this->handshake_);
    this->handshake_ = nullptr;
  }
}

int NoiseResponderHandshake::init(const psk_t &psk, const uint8_t *prologue, size_t prologue_len) {
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
  if (err != 0)
    return err;
  err = noise_handshakestate_set_pre_shared_key(this->handshake_, psk.data(), psk.size());
  if (err != 0)
    return err;
  err = noise_handshakestate_set_prologue(this->handshake_, prologue, prologue_len);
  if (err != 0)
    return err;
  return noise_handshakestate_start(this->handshake_);
}

NoiseResponderHandshake::Action NoiseResponderHandshake::action() const {
  switch (noise_handshakestate_get_action(this->handshake_)) {
    case NOISE_ACTION_READ_MESSAGE:
      return Action::READ;
    case NOISE_ACTION_WRITE_MESSAGE:
      return Action::WRITE;
    case NOISE_ACTION_SPLIT:
      return Action::SPLIT;
    default:
      return Action::FAILED;
  }
}

int NoiseResponderHandshake::read_message(const uint8_t *data, size_t len) {
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_input(mbuf, const_cast<uint8_t *>(data), len);
  return noise_handshakestate_read_message(this->handshake_, &mbuf, nullptr);
}

int NoiseResponderHandshake::write_message(uint8_t *out, size_t capacity, size_t &out_len) {
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_output(mbuf, out, capacity);
  int err = noise_handshakestate_write_message(this->handshake_, &mbuf, nullptr);
  if (err == 0)
    out_len = mbuf.size;
  return err;
}

int NoiseResponderHandshake::split(NoiseCipherState *&send_cipher, NoiseCipherState *&recv_cipher) {
  int err = noise_handshakestate_split(this->handshake_, &send_cipher, &recv_cipher);
  if (err != 0)
    return err;
  noise_handshakestate_free(this->handshake_);
  this->handshake_ = nullptr;
  return 0;
}

}  // namespace esphome::noise
#endif  // USE_NOISE
