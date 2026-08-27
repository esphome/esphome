#include <gtest/gtest.h>

#include <cstring>

#include <noise/protocol.h>

#include "esphome/components/noise/noise.h"
#include "esphome/components/noise/noise_handshake.h"

namespace esphome::noise::testing {

using Action = NoiseResponderHandshake::Action;

// A raw noise-c initiator driving the same Noise_NNpsk0_25519_ChaChaPoly_SHA256
// pattern the responder class implements, so the tests exercise a real
// two-message handshake rather than mirrored calls into the class under test.
class Initiator {
 public:
  Initiator(const psk_t &psk, const uint8_t *prologue, size_t prologue_len) {
    const NoiseProtocolId nid = {
        .prefix_id = NOISE_PREFIX_STANDARD,
        .pattern_id = NOISE_PATTERN_NN,
        .modifier_ids = {NOISE_MODIFIER_PSK0},
        .dh_id = NOISE_DH_CURVE25519,
        .cipher_id = NOISE_CIPHER_CHACHAPOLY,
        .hash_id = NOISE_HASH_SHA256,
        .hybrid_id = NOISE_DH_NONE,
    };
    EXPECT_EQ(noise_handshakestate_new_by_id(&this->state_, &nid, NOISE_ROLE_INITIATOR), 0);
    EXPECT_EQ(noise_handshakestate_set_pre_shared_key(this->state_, psk.data(), psk.size()), 0);
    EXPECT_EQ(noise_handshakestate_set_prologue(this->state_, prologue, prologue_len), 0);
    EXPECT_EQ(noise_handshakestate_start(this->state_), 0);
  }
  ~Initiator() {
    if (this->state_ != nullptr)
      noise_handshakestate_free(this->state_);
    if (this->send_ != nullptr)
      noise_cipherstate_free(this->send_);
    if (this->recv_ != nullptr)
      noise_cipherstate_free(this->recv_);
  }
  Initiator(const Initiator &) = delete;
  Initiator &operator=(const Initiator &) = delete;

  size_t write_message(uint8_t *out, size_t capacity) {
    NoiseBuffer mbuf;
    noise_buffer_init(mbuf);
    noise_buffer_set_output(mbuf, out, capacity);
    EXPECT_EQ(noise_handshakestate_write_message(this->state_, &mbuf, nullptr), 0);
    return mbuf.size;
  }

  int read_message(uint8_t *data, size_t len) {
    NoiseBuffer mbuf;
    noise_buffer_init(mbuf);
    noise_buffer_set_input(mbuf, data, len);
    return noise_handshakestate_read_message(this->state_, &mbuf, nullptr);
  }

  void split() { EXPECT_EQ(noise_handshakestate_split(this->state_, &this->send_, &this->recv_), 0); }

  NoiseCipherState *send_{nullptr};
  NoiseCipherState *recv_{nullptr};

 private:
  NoiseHandshakeState *state_{nullptr};
};

static const uint8_t PROLOGUE[] = {'t', 'e', 's', 't', 'p', 'r', 'o', 'l', 'o', 'g', 'u', 'e'};

static psk_t make_psk(uint8_t seed) {
  psk_t psk;
  for (size_t i = 0; i < psk.size(); i++) {
    psk[i] = static_cast<uint8_t>(seed + i);
  }
  return psk;
}

TEST(NoiseResponderHandshakeTest, ActionFailedBeforeInit) {
  NoiseResponderHandshake handshake;
  EXPECT_EQ(handshake.action(), Action::ACTION_FAILED);
}

TEST(NoiseResponderHandshakeTest, MessageMethodsErrorBeforeInit) {
  // The class doc promises a noise-c error, not a crash, when the message
  // methods run outside their action() step; pin the library's null check
  NoiseResponderHandshake handshake;
  uint8_t buf[MAX_HANDSHAKE_SIZE] = {};
  size_t out_len = 0;
  EXPECT_NE(handshake.read_message(buf, sizeof(buf)), 0);
  EXPECT_NE(handshake.write_message(buf, sizeof(buf), out_len), 0);
  // Deliberately non-null: split() documents a nullptr postcondition on
  // error, so a caller's uninitialized locals never hold garbage to free
  auto *sentinel = reinterpret_cast<NoiseCipherState *>(0x1);
  NoiseCipherState *send_cipher = sentinel;
  NoiseCipherState *recv_cipher = sentinel;
  EXPECT_NE(handshake.split(send_cipher, recv_cipher), 0);
  EXPECT_EQ(send_cipher, nullptr);
  EXPECT_EQ(recv_cipher, nullptr);
}

TEST(NoiseResponderHandshakeTest, FullHandshakeAndTransportRoundTrip) {
  const psk_t psk = make_psk(7);
  NoiseResponderHandshake responder;
  ASSERT_EQ(responder.init(psk, PROLOGUE, sizeof(PROLOGUE)), 0);
  EXPECT_EQ(responder.action(), Action::ACTION_READ);

  Initiator initiator(psk, PROLOGUE, sizeof(PROLOGUE));
  uint8_t msg[MAX_HANDSHAKE_SIZE];
  size_t msg_len = initiator.write_message(msg, sizeof(msg));
  ASSERT_GT(msg_len, 0u);

  ASSERT_EQ(responder.read_message(msg, msg_len), 0);
  ASSERT_EQ(responder.action(), Action::ACTION_WRITE);

  size_t reply_len = 0;
  ASSERT_EQ(responder.write_message(msg, sizeof(msg), reply_len), 0);
  ASSERT_GT(reply_len, 0u);
  ASSERT_EQ(responder.action(), Action::ACTION_SPLIT);

  ASSERT_EQ(initiator.read_message(msg, reply_len), 0);
  initiator.split();

  NoiseCipherState *send_cipher = nullptr;
  NoiseCipherState *recv_cipher = nullptr;
  ASSERT_EQ(responder.split(send_cipher, recv_cipher), 0);
  ASSERT_NE(send_cipher, nullptr);
  ASSERT_NE(recv_cipher, nullptr);
  // The handshake state is released by split(); the class reports FAILED after
  EXPECT_EQ(responder.action(), Action::ACTION_FAILED);
  EXPECT_EQ(static_cast<size_t>(noise_cipherstate_get_mac_length(send_cipher)), MAC_SIZE);

  // Responder encrypts, initiator decrypts
  uint8_t frame[64];
  static constexpr char PLAINTEXT[] = "encrypted ota";
  std::memcpy(frame, PLAINTEXT, sizeof(PLAINTEXT));
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, frame, sizeof(PLAINTEXT), sizeof(frame));
  ASSERT_EQ(noise_cipherstate_encrypt(send_cipher, &mbuf), 0);
  EXPECT_EQ(mbuf.size, sizeof(PLAINTEXT) + MAC_SIZE);

  noise_buffer_set_inout(mbuf, frame, mbuf.size, sizeof(frame));
  ASSERT_EQ(noise_cipherstate_decrypt(initiator.recv_, &mbuf), 0);
  ASSERT_EQ(mbuf.size, sizeof(PLAINTEXT));
  EXPECT_EQ(std::memcmp(frame, PLAINTEXT, sizeof(PLAINTEXT)), 0);

  noise_cipherstate_free(send_cipher);
  noise_cipherstate_free(recv_cipher);
}

TEST(NoiseResponderHandshakeTest, ReInitRestartsHandshake) {
  // The documented retry shape: a repeated init() frees the previous state
  // and starts over. The first message under the new key authenticating
  // proves the restart took effect; the old state surviving would fail the
  // MAC here.
  NoiseResponderHandshake responder;
  ASSERT_EQ(responder.init(make_psk(7), PROLOGUE, sizeof(PROLOGUE)), 0);
  ASSERT_EQ(responder.init(make_psk(9), PROLOGUE, sizeof(PROLOGUE)), 0);
  EXPECT_EQ(responder.action(), Action::ACTION_READ);

  Initiator initiator(make_psk(9), PROLOGUE, sizeof(PROLOGUE));
  uint8_t msg[MAX_HANDSHAKE_SIZE];
  size_t msg_len = initiator.write_message(msg, sizeof(msg));
  ASSERT_GT(msg_len, 0u);
  EXPECT_EQ(responder.read_message(msg, msg_len), 0);
}

TEST(NoiseResponderHandshakeTest, WrongPskFailsWithMacFailure) {
  NoiseResponderHandshake responder;
  ASSERT_EQ(responder.init(make_psk(7), PROLOGUE, sizeof(PROLOGUE)), 0);

  Initiator initiator(make_psk(200), PROLOGUE, sizeof(PROLOGUE));
  uint8_t msg[MAX_HANDSHAKE_SIZE];
  size_t msg_len = initiator.write_message(msg, sizeof(msg));
  ASSERT_GT(msg_len, 0u);

  int err = responder.read_message(msg, msg_len);
  EXPECT_EQ(err, NOISE_ERROR_MAC_FAILURE);
  EXPECT_EQ(responder.action(), Action::ACTION_FAILED);
}

TEST(NoiseResponderHandshakeTest, MismatchedPrologueFailsWithMacFailure) {
  // The prologue binds the plaintext preamble for downgrade resistance; a
  // tampered preamble must fail even with the right key.
  const psk_t psk = make_psk(7);
  NoiseResponderHandshake responder;
  ASSERT_EQ(responder.init(psk, PROLOGUE, sizeof(PROLOGUE)), 0);

  static const uint8_t TAMPERED[] = {'x'};
  Initiator initiator(psk, TAMPERED, sizeof(TAMPERED));
  uint8_t msg[MAX_HANDSHAKE_SIZE];
  size_t msg_len = initiator.write_message(msg, sizeof(msg));
  ASSERT_GT(msg_len, 0u);

  EXPECT_EQ(responder.read_message(msg, msg_len), NOISE_ERROR_MAC_FAILURE);
}

}  // namespace esphome::noise::testing
