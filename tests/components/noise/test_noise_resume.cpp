#include <gtest/gtest.h>

#include <cstring>

#include <noise/protocol.h>

#include "esphome/components/noise/noise.h"
#include "esphome/components/noise/noise_resume.h"

namespace esphome::noise::testing {

// Known-answer vectors shared with the client implementation
// (aioesphomeapi tests/test_noise_resume.py); the two must stay identical
// byte for byte or resumed sessions cannot interoperate.
static const uint8_t KAT_SECRET[RESUME_SECRET_SIZE] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
static const uint8_t KAT_SESSION_ID[RESUME_SESSION_ID_SIZE] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7};
static const uint8_t KAT_CLIENT_NONCE[RESUME_NONCE_SIZE] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                                            0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
static const uint8_t KAT_SERVER_NONCE[RESUME_NONCE_SIZE] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                                                            0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f};
static const uint8_t KAT_OFFER_MAC[RESUME_MAC_SIZE] = {0xa8, 0x08, 0xea, 0xdb, 0xec, 0x81, 0xa7, 0xcb,
                                                       0xf4, 0xca, 0xaa, 0xb8, 0x0d, 0x7f, 0x9d, 0x01};
static const uint8_t KAT_CONFIRM_MAC[RESUME_MAC_SIZE] = {0x09, 0xa3, 0x70, 0x3e, 0xc8, 0x34, 0x77, 0xe9,
                                                         0x45, 0xe7, 0xf1, 0x61, 0x9d, 0x4f, 0x6a, 0x76};
static const uint8_t KAT_K_C2D[32] = {0xd6, 0x01, 0xe3, 0xc1, 0x16, 0xa1, 0x64, 0x66, 0xdb, 0xc5, 0x9e,
                                      0xdd, 0x60, 0x2a, 0x64, 0x1e, 0xbe, 0xf5, 0x11, 0x95, 0x98, 0xd2,
                                      0xf2, 0x47, 0x1b, 0xc6, 0x8c, 0x51, 0x8f, 0xbe, 0xb7, 0x23};
static const uint8_t KAT_K_D2C[32] = {0x7f, 0x8d, 0x57, 0x7e, 0x9f, 0xb4, 0xbb, 0xde, 0x86, 0xcd, 0xa9,
                                      0xf4, 0x9b, 0x42, 0xe7, 0x24, 0xc8, 0x49, 0xce, 0x89, 0xd8, 0x96,
                                      0x3f, 0x3c, 0x4b, 0x3f, 0x8f, 0x80, 0xc2, 0x56, 0xab, 0x65};

/// The one place in this file that spells the offer wire layout
static void build_offer(uint8_t *offer, const uint8_t *session_id, const uint8_t *client_nonce, const uint8_t *mac) {
  offer[0] = RESUME_OFFER_VERSION;
  std::memcpy(offer + RESUME_OFFER_SESSION_ID_OFFSET, session_id, RESUME_SESSION_ID_SIZE);
  std::memcpy(offer + RESUME_OFFER_NONCE_OFFSET, client_nonce, RESUME_NONCE_SIZE);
  std::memcpy(offer + RESUME_OFFER_MAC_OFFSET, mac, RESUME_MAC_SIZE);
}

/// "NoiseAPIInit" || be16(len) || offer, exactly as the api frame helper mixes it
static constexpr size_t KAT_PROLOGUE_SIZE = 12 + 2 + RESUME_OFFER_SIZE;
static void build_prologue(uint8_t *out, const uint8_t *offer) {
  std::memcpy(out, "NoiseAPIInit", 12);  // NOLINT(bugprone-not-null-terminated-result)
  out[12] = 0x00;
  out[13] = RESUME_OFFER_SIZE;
  std::memcpy(out + 14, offer, RESUME_OFFER_SIZE);
}

static void build_offer_for_ticket(uint8_t *offer, const ResumeTicket &ticket, const uint8_t *client_nonce) {
  uint8_t mac[RESUME_MAC_SIZE];
  ASSERT_TRUE(resume_compute_offer_mac(ticket.secret, ticket.session_id, client_nonce, mac));
  build_offer(offer, ticket.session_id, client_nonce, mac);
}

/// Test access to the protected slots so a test can plant the KAT ticket
struct TestCache : ResumeTicketCache {
  void plant(const uint8_t *session_id, const uint8_t *secret) {
    std::memcpy(this->slots_[0].session_id, session_id, RESUME_SESSION_ID_SIZE);
    std::memcpy(this->slots_[0].secret, secret, RESUME_SECRET_SIZE);
    this->used_mask_ |= 1u;
  }
};

TEST(NoiseResumeKat, ConfirmMacMatchesClientImplementation) {
  uint8_t mac[RESUME_MAC_SIZE];
  ASSERT_TRUE(resume_compute_confirm_mac(KAT_SECRET, KAT_CLIENT_NONCE, KAT_SERVER_NONCE, mac));
  EXPECT_EQ(std::memcmp(mac, KAT_CONFIRM_MAC, RESUME_MAC_SIZE), 0);
}

TEST(NoiseResumeKat, OfferMacMatchesClientImplementation) {
  uint8_t mac[RESUME_MAC_SIZE];
  ASSERT_TRUE(resume_compute_offer_mac(KAT_SECRET, KAT_SESSION_ID, KAT_CLIENT_NONCE, mac));
  EXPECT_EQ(std::memcmp(mac, KAT_OFFER_MAC, RESUME_MAC_SIZE), 0);
}

TEST(NoiseResumeKat, KeyDerivationMatchesClientImplementation) {
  // Prologue used by the shared vectors: "NoiseAPIInit" + be16(41) + a
  // 41-byte offer whose MAC field is 16 bytes of 0xEE
  uint8_t mac_filler[RESUME_MAC_SIZE];
  std::memset(mac_filler, 0xEE, sizeof(mac_filler));
  uint8_t offer[RESUME_OFFER_SIZE];
  build_offer(offer, KAT_SESSION_ID, KAT_CLIENT_NONCE, mac_filler);
  uint8_t prologue[KAT_PROLOGUE_SIZE];
  build_prologue(prologue, offer);

  uint8_t k_c2d[32], k_d2c[32];
  ASSERT_TRUE(
      resume_derive_keys(KAT_SECRET, KAT_CLIENT_NONCE, KAT_SERVER_NONCE, prologue, sizeof(prologue), k_c2d, k_d2c));
  EXPECT_EQ(std::memcmp(k_c2d, KAT_K_C2D, 32), 0);
  EXPECT_EQ(std::memcmp(k_d2c, KAT_K_D2C, 32), 0);
}

TEST(NoiseResumeCache, TryAcceptConsumesTicketOnceAndProvesPossession) {
  TestCache cache;
  cache.plant(KAT_SESSION_ID, KAT_SECRET);

  uint8_t offer[RESUME_OFFER_SIZE];
  build_offer(offer, KAT_SESSION_ID, KAT_CLIENT_NONCE, KAT_OFFER_MAC);
  uint8_t prologue[KAT_PROLOGUE_SIZE];
  build_prologue(prologue, offer);

  uint8_t ext[RESUME_ACCEPT_SIZE];
  NoiseCipherState *send = nullptr, *recv = nullptr;
  ASSERT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv),
            RESUME_ACCEPT_SIZE);
  ASSERT_NE(send, nullptr);
  ASSERT_NE(recv, nullptr);

  // The extension proves possession: verify like the client does
  EXPECT_EQ(ext[0], RESUME_ACCEPT_VERSION);
  const uint8_t *server_nonce = ext + 1;
  uint8_t expected_confirm[RESUME_MAC_SIZE];
  ASSERT_TRUE(resume_compute_confirm_mac(KAT_SECRET, KAT_CLIENT_NONCE, server_nonce, expected_confirm));
  EXPECT_EQ(std::memcmp(ext + 1 + RESUME_NONCE_SIZE, expected_confirm, RESUME_MAC_SIZE), 0);

  // The ciphers must interoperate with the documented key derivation
  uint8_t k_c2d[32], k_d2c[32];
  ASSERT_TRUE(resume_derive_keys(KAT_SECRET, KAT_CLIENT_NONCE, server_nonce, prologue, sizeof(prologue), k_c2d, k_d2c));
  NoiseCipherState *client_send = resume_make_cipher(k_c2d);
  ASSERT_NE(client_send, nullptr);
  uint8_t buf[64] = "resumed";
  NoiseBuffer nb;
  noise_buffer_init(nb);
  noise_buffer_set_inout(nb, buf, 7, sizeof(buf));
  ASSERT_EQ(noise_cipherstate_encrypt(client_send, &nb), NOISE_ERROR_NONE);
  ASSERT_EQ(noise_cipherstate_decrypt(recv, &nb), NOISE_ERROR_NONE);
  EXPECT_EQ(std::memcmp(buf, "resumed", 7), 0);
  noise_cipherstate_free(client_send);
  noise_cipherstate_free(send);
  noise_cipherstate_free(recv);

  // Single use: the same offer must miss the second time
  NoiseCipherState *send2 = nullptr, *recv2 = nullptr;
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send2, recv2), 0u);
  EXPECT_EQ(send2, nullptr);
  EXPECT_EQ(recv2, nullptr);
}

TEST(NoiseResumeCache, BadMacOrMalformedOfferLeavesTicketIntact) {
  TestCache cache;
  cache.plant(KAT_SESSION_ID, KAT_SECRET);

  uint8_t offer[RESUME_OFFER_SIZE];
  uint8_t bad_mac[RESUME_MAC_SIZE];
  std::memcpy(bad_mac, KAT_OFFER_MAC, RESUME_MAC_SIZE);
  bad_mac[0] ^= 0x01;
  build_offer(offer, KAT_SESSION_ID, KAT_CLIENT_NONCE, bad_mac);

  uint8_t prologue[1] = {0};
  uint8_t ext[RESUME_ACCEPT_SIZE];
  NoiseCipherState *send = nullptr, *recv = nullptr;
  // A forged offer must not burn the ticket
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv), 0u);
  // Wrong size or version must be recognized as "no offer"
  build_offer(offer, KAT_SESSION_ID, KAT_CLIENT_NONCE, KAT_OFFER_MAC);
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer) - 1, prologue, sizeof(prologue), ext, sizeof(ext), send, recv), 0u);
  offer[0] = 0x7f;
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv), 0u);
  offer[0] = RESUME_OFFER_VERSION;
  // No room for the extension must also decline without burning it
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext) - 1, send, recv), 0u);
  // The genuine offer still redeems
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv),
            RESUME_ACCEPT_SIZE);
  noise_cipherstate_free(send);
  noise_cipherstate_free(recv);
}

TEST(NoiseResumeCache, IssueRotatesSlotsAndClearForgetsAll) {
  ResumeTicketCache cache;
  ResumeTicket tickets[ResumeTicketCache::SLOTS + 1];
  for (auto &ticket : tickets) {
    ASSERT_TRUE(cache.issue(ticket));
  }
  uint8_t offer[RESUME_OFFER_SIZE];
  uint8_t prologue[1] = {0};
  uint8_t ext[RESUME_ACCEPT_SIZE];

  // The oldest ticket was evicted by the one-past-capacity issue
  build_offer_for_ticket(offer, tickets[0], KAT_CLIENT_NONCE);
  NoiseCipherState *send = nullptr, *recv = nullptr;
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv), 0u);
  // The rest remain redeemable
  for (int i = 1; i <= ResumeTicketCache::SLOTS; i++) {
    build_offer_for_ticket(offer, tickets[i], KAT_CLIENT_NONCE);
    EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv),
              RESUME_ACCEPT_SIZE);
    noise_cipherstate_free(send);
    noise_cipherstate_free(recv);
    send = recv = nullptr;
  }

  // clear() forgets everything
  ResumeTicket ticket;
  ASSERT_TRUE(cache.issue(ticket));
  cache.clear();
  build_offer_for_ticket(offer, ticket, KAT_CLIENT_NONCE);
  EXPECT_EQ(cache.try_accept(offer, sizeof(offer), prologue, sizeof(prologue), ext, sizeof(ext), send, recv), 0u);
}

}  // namespace esphome::noise::testing
