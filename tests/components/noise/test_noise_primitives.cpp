#include <gtest/gtest.h>

#include <cstring>

#include <noise/protocol.h>

#include "esphome/components/noise/noise.h"

namespace esphome::noise::testing {

TEST(NoiseContextTest, AllZerosPskIsReserved) {
  psk_t zeros{};
  EXPECT_TRUE(NoiseContext::is_all_zeros(zeros));

  psk_t psk{};
  psk[31] = 1;
  EXPECT_FALSE(NoiseContext::is_all_zeros(psk));

  NoiseContext ctx;
  EXPECT_FALSE(ctx.has_psk());
  ctx.set_psk(zeros);
  EXPECT_FALSE(ctx.has_psk());
  ctx.set_psk(psk);
  EXPECT_TRUE(ctx.has_psk());
  EXPECT_EQ(ctx.get_psk(), psk);
}

TEST(WireFormatTest, FrameHeaderIsIndicatorPlusBigEndianLength) {
  uint8_t header[FRAME_HEADER_SIZE];
  write_frame_header(header, 0x1234);
  EXPECT_EQ(header[0], FRAME_INDICATOR);
  EXPECT_EQ(header[1], 0x12);
  EXPECT_EQ(header[2], 0x34);
}

TEST(WireFormatTest, RejectPayloadCarriesStatusByteAndMacFailureContract) {
  // The MAC failure string is a wire contract: clients match it to report a
  // wrong key. Format the payload exactly the way the handshake read path does.
  uint8_t buf[64];
  size_t len = format_reject_payload(buf, sizeof(buf), reject_reason_for(NOISE_ERROR_MAC_FAILURE));
  static constexpr char EXPECTED[] = "Handshake MAC failure";
  ASSERT_EQ(len, 1 + strlen(EXPECTED));
  EXPECT_EQ(buf[0], HANDSHAKE_STATUS_REJECT);
  EXPECT_EQ(memcmp(buf + 1, EXPECTED, strlen(EXPECTED)), 0);
  // The exported floor covers the full MAC failure payload exactly
  EXPECT_EQ(MAC_FAILURE_PAYLOAD_SIZE, 1 + strlen(EXPECTED));

  // Any other error maps to the generic reason
  len = format_reject_payload(buf, sizeof(buf), reject_reason_for(NOISE_ERROR_INVALID_STATE));
  static constexpr char GENERIC[] = "Handshake error";
  ASSERT_EQ(len, 1 + strlen(GENERIC));
  EXPECT_EQ(memcmp(buf + 1, GENERIC, strlen(GENERIC)), 0);
}

TEST(WireFormatTest, RejectPayloadTruncatesToCapacity) {
  uint8_t buf[8];
  size_t len = format_reject_payload(buf, sizeof(buf), reject_reason_for(NOISE_ERROR_MAC_FAILURE));
  ASSERT_EQ(len, sizeof(buf));
  EXPECT_EQ(buf[0], HANDSHAKE_STATUS_REJECT);
  EXPECT_EQ(memcmp(buf + 1, "Handsha", 7), 0);

  // A one-byte buffer still carries the status byte
  uint8_t tiny[1];
  len = format_reject_payload(tiny, sizeof(tiny), reject_reason_for(NOISE_ERROR_MAC_FAILURE));
  ASSERT_EQ(len, 1u);
  EXPECT_EQ(tiny[0], HANDSHAKE_STATUS_REJECT);

  // A zero-capacity buffer yields no payload and stays untouched
  uint8_t none[1] = {0xAA};
  EXPECT_EQ(format_reject_payload(none, 0, reject_reason_for(NOISE_ERROR_MAC_FAILURE)), 0u);
  EXPECT_EQ(none[0], 0xAA);
}

}  // namespace esphome::noise::testing
