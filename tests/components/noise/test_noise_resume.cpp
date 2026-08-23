#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/noise/noise.h"
#include "esphome/components/noise/noise_resume.h"

namespace esphome::noise::testing {

// Known-answer vectors shared with the client implementation
// (aioesphomeapi tests/test_noise_resume.py); the two must stay identical
// byte for byte or resumed sessions cannot interoperate.
static const uint8_t KAT_SECRET[RESUME_SECRET_SIZE] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
static const uint8_t KAT_SESSION_ID[RESUME_SESSION_ID_SIZE] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7};

static void fill_nonces(uint8_t *client_nonce, uint8_t *server_nonce) {
  for (int i = 0; i < 16; i++) {
    client_nonce[i] = 0x10 + i;
    server_nonce[i] = 0x30 + i;
  }
}

static void build_kat_offer(uint8_t *offer, const uint8_t *client_nonce, const uint8_t *offer_mac) {
  offer[0] = RESUME_OFFER_VERSION;
  std::memcpy(offer + RESUME_OFFER_SESSION_ID_OFFSET, KAT_SESSION_ID, RESUME_SESSION_ID_SIZE);
  std::memcpy(offer + RESUME_OFFER_NONCE_OFFSET, client_nonce, RESUME_NONCE_SIZE);
  std::memcpy(offer + RESUME_OFFER_MAC_OFFSET, offer_mac, RESUME_MAC_SIZE);
}

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

TEST(NoiseResumeKat, ConfirmMacMatchesClientImplementation) {
  uint8_t client_nonce[16], server_nonce[16], mac[RESUME_MAC_SIZE];
  fill_nonces(client_nonce, server_nonce);
  ASSERT_TRUE(resume_compute_confirm_mac(KAT_SECRET, client_nonce, server_nonce, mac));
  EXPECT_EQ(std::memcmp(mac, KAT_CONFIRM_MAC, RESUME_MAC_SIZE), 0);
}

TEST(NoiseResumeKat, KeyDerivationMatchesClientImplementation) {
  uint8_t client_nonce[16], server_nonce[16];
  fill_nonces(client_nonce, server_nonce);
  // Prologue used by the shared vectors: "NoiseAPIInit" + be16(41) + a
  // 41-byte offer whose MAC field is 16 bytes of 0xEE
  uint8_t prologue[55];
  std::memcpy(prologue, "NoiseAPIInit", 12);
  prologue[12] = 0x00;
  prologue[13] = 0x29;
  uint8_t mac_filler[RESUME_MAC_SIZE];
  std::memset(mac_filler, 0xEE, sizeof(mac_filler));
  build_kat_offer(prologue + 14, client_nonce, mac_filler);

  uint8_t k_c2d[32], k_d2c[32];
  ASSERT_TRUE(resume_derive_keys(KAT_SECRET, client_nonce, server_nonce, prologue, sizeof(prologue), k_c2d, k_d2c));
  EXPECT_EQ(std::memcmp(k_c2d, KAT_K_C2D, 32), 0);
  EXPECT_EQ(std::memcmp(k_d2c, KAT_K_D2C, 32), 0);
}

TEST(NoiseResumeCache, TakeVerifiedConsumesTicketOnce) {
  ResumeTicketCache cache;
  // Plant the KAT ticket directly through the protected members via a
  // subclass so the test controls the session id and secret.
  struct TestCache : ResumeTicketCache {
    void plant(const uint8_t *session_id, const uint8_t *secret) {
      std::memcpy(this->slots_[0].session_id, session_id, RESUME_SESSION_ID_SIZE);
      std::memcpy(this->slots_[0].secret, secret, RESUME_SECRET_SIZE);
      this->slots_[0].valid = true;
    }
  } test_cache;
  test_cache.plant(KAT_SESSION_ID, KAT_SECRET);

  uint8_t client_nonce[16], server_nonce[16];
  fill_nonces(client_nonce, server_nonce);
  uint8_t offer[RESUME_OFFER_SIZE];
  build_kat_offer(offer, client_nonce, KAT_OFFER_MAC);

  uint8_t secret[RESUME_SECRET_SIZE];
  ASSERT_TRUE(test_cache.take_verified(offer, secret));
  EXPECT_EQ(std::memcmp(secret, KAT_SECRET, RESUME_SECRET_SIZE), 0);
  // Single use: the same offer must miss the second time
  EXPECT_FALSE(test_cache.take_verified(offer, secret));
}

TEST(NoiseResumeCache, BadMacLeavesTicketIntact) {
  struct TestCache : ResumeTicketCache {
    void plant(const uint8_t *session_id, const uint8_t *secret) {
      std::memcpy(this->slots_[0].session_id, session_id, RESUME_SESSION_ID_SIZE);
      std::memcpy(this->slots_[0].secret, secret, RESUME_SECRET_SIZE);
      this->slots_[0].valid = true;
    }
  } test_cache;
  test_cache.plant(KAT_SESSION_ID, KAT_SECRET);

  uint8_t client_nonce[16], server_nonce[16];
  fill_nonces(client_nonce, server_nonce);
  uint8_t offer[RESUME_OFFER_SIZE];
  uint8_t bad_mac[RESUME_MAC_SIZE];
  std::memcpy(bad_mac, KAT_OFFER_MAC, RESUME_MAC_SIZE);
  bad_mac[0] ^= 0x01;
  build_kat_offer(offer, client_nonce, bad_mac);

  uint8_t secret[RESUME_SECRET_SIZE];
  // A forged offer must not burn the ticket
  EXPECT_FALSE(test_cache.take_verified(offer, secret));
  build_kat_offer(offer, client_nonce, KAT_OFFER_MAC);
  EXPECT_TRUE(test_cache.take_verified(offer, secret));
}

static void build_offer_for_ticket(uint8_t *offer, const ResumeTicket &ticket, const uint8_t *client_nonce) {
  offer[0] = RESUME_OFFER_VERSION;
  std::memcpy(offer + RESUME_OFFER_SESSION_ID_OFFSET, ticket.session_id, RESUME_SESSION_ID_SIZE);
  std::memcpy(offer + RESUME_OFFER_NONCE_OFFSET, client_nonce, RESUME_NONCE_SIZE);
  ASSERT_TRUE(
      resume_compute_offer_mac(ticket.secret, ticket.session_id, client_nonce, offer + RESUME_OFFER_MAC_OFFSET));
}

TEST(NoiseResumeCache, IssueRotatesSlotsAndClearForgetsAll) {
  ResumeTicketCache cache;
  ResumeTicket tickets[5];
  for (auto &ticket : tickets) {
    ASSERT_TRUE(cache.issue(ticket));
    ASSERT_TRUE(ticket.valid);
  }
  uint8_t client_nonce[16], server_nonce[16];
  fill_nonces(client_nonce, server_nonce);
  uint8_t offer[RESUME_OFFER_SIZE];
  uint8_t secret[RESUME_SECRET_SIZE];

  // Slot 0 was evicted by the fifth issue
  build_offer_for_ticket(offer, tickets[0], client_nonce);
  EXPECT_FALSE(cache.take_verified(offer, secret));
  // Tickets 1..4 remain redeemable
  for (int i = 1; i < 5; i++) {
    build_offer_for_ticket(offer, tickets[i], client_nonce);
    EXPECT_TRUE(cache.take_verified(offer, secret));
    EXPECT_EQ(std::memcmp(secret, tickets[i].secret, RESUME_SECRET_SIZE), 0);
  }

  // clear() forgets everything
  ResumeTicket ticket;
  ASSERT_TRUE(cache.issue(ticket));
  cache.clear();
  build_offer_for_ticket(offer, ticket, client_nonce);
  EXPECT_FALSE(cache.take_verified(offer, secret));
}

}  // namespace esphome::noise::testing
