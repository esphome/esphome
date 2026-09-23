#include <gtest/gtest.h>

#include <cstdint>
#include <ios>
#include <random>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/proto.h"

namespace esphome::api::testing {

// Generic varint decoder, used to verify the encoded bytes round-trip back to
// the original 48-bit MAC value, independent of the specialized encoder under
// test.
static uint64_t decode_varint(const uint8_t *buf, size_t len, size_t *consumed) {
  uint64_t value = 0;
  int shift = 0;
  for (size_t i = 0; i < len; i++) {
    value |= static_cast<uint64_t>(buf[i] & 0x7F) << shift;
    if ((buf[i] & 0x80) == 0) {
      *consumed = i + 1;
      return value;
    }
    shift += 7;
  }
  *consumed = 0;
  return 0;
}

// Reference encoder mirroring ProtoEncode::encode_varint_raw_64.
static size_t reference_encode(uint64_t value, uint8_t *out) {
  uint8_t *p = out;
  if (value < 128) {
    *p++ = static_cast<uint8_t>(value);
    return p - out;
  }
  do {
    *p++ = static_cast<uint8_t>(value | 0x80);
    value >>= 7;
  } while (value > 0x7F);
  *p++ = static_cast<uint8_t>(value);
  return p - out;
}

// Encode `mac` via the 48-bit fast path and verify:
//  - byte-identical output to the reference loop
//  - encoded byte length matches `expected_bytes`
//  - calc_uint64_48bit_force agrees on the size
//  - the bytes round-trip through a generic varint decoder
static void verify_mac(uint64_t mac, size_t expected_bytes) {
  ASSERT_LT(mac, 1ULL << 48) << "test fixture mac exceeds 48 bits";

  uint8_t ref_buf[16] = {0};
  size_t ref_len = reference_encode(mac, ref_buf);

  APIBuffer api_buf;
  api_buf.resize(16);
  uint8_t *pos = api_buf.data();
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = api_buf.data() + api_buf.size();
#endif
  ProtoEncode::encode_varint_raw_48bit(pos PROTO_ENCODE_DEBUG_ARG, mac);
  size_t new_len = pos - api_buf.data();

  EXPECT_EQ(new_len, expected_bytes) << "mac=0x" << std::hex << mac << std::dec;
  EXPECT_EQ(ref_len, expected_bytes) << "reference disagrees on length for mac=0x" << std::hex << mac << std::dec;

  for (size_t i = 0; i < new_len; i++) {
    EXPECT_EQ(api_buf.data()[i], ref_buf[i])
        << "byte " << i << " differs for mac=0x" << std::hex << mac << " (got 0x" << static_cast<int>(api_buf.data()[i])
        << ", expected 0x" << static_cast<int>(ref_buf[i]) << ")" << std::dec;
  }

  size_t consumed = 0;
  uint64_t decoded = decode_varint(api_buf.data(), new_len, &consumed);
  EXPECT_EQ(consumed, new_len) << "decoder did not consume all bytes for mac=0x" << std::hex << mac << std::dec;
  EXPECT_EQ(decoded, mac) << "round-trip mismatch for mac=0x" << std::hex << mac << std::dec;

  // Verify the size helper agrees. field_id_size = 1 (typical 1-byte tag).
  uint32_t calc_size = ProtoSize::calc_uint64_48bit_force(1, mac);
  EXPECT_EQ(calc_size, 1 + expected_bytes)
      << "calc_uint64_48bit_force size mismatch for mac=0x" << std::hex << mac << std::dec;
}

// Compute the canonical varint byte length for a value < 1<<48.
static size_t expected_varint_len(uint64_t v) {
  if (v < (1ULL << 7))
    return 1;
  if (v < (1ULL << 14))
    return 2;
  if (v < (1ULL << 21))
    return 3;
  if (v < (1ULL << 28))
    return 4;
  if (v < (1ULL << 35))
    return 5;
  if (v < (1ULL << 42))
    return 6;
  return 7;
}

// --- Specific MACs requested for verification ---

TEST(ProtoMacVarint, AllZeros) { verify_mac(0x000000000000ULL, 1); }        // 00:00:00:00:00:00
TEST(ProtoMacVarint, FirstByteOnly) { verify_mac(0x110000000000ULL, 7); }   // 11:00:00:00:00:00
TEST(ProtoMacVarint, SecondByteOnly) { verify_mac(0x00AA00000000ULL, 6); }  // 00:AA:00:00:00:00
TEST(ProtoMacVarint, ThirdByteOnly) { verify_mac(0x0000BB000000ULL, 5); }   // 00:00:BB:00:00:00
TEST(ProtoMacVarint, FourthByteOnly) { verify_mac(0x000000CC0000ULL, 4); }  // 00:00:00:CC:00:00
TEST(ProtoMacVarint, FifthByteOnly) { verify_mac(0x00000000DD00ULL, 3); }   // 00:00:00:00:DD:00
TEST(ProtoMacVarint, SixthByteOnly) { verify_mac(0x0000000000EEULL, 2); }   // 00:00:00:00:00:EE
TEST(ProtoMacVarint, AllOnes) { verify_mac(0xFFFFFFFFFFFFULL, 7); }         // FF:FF:FF:FF:FF:FF

// 100 deterministic-random 48-bit MACs to catch regressions across the space.
TEST(ProtoMacVarint, RandomSample) {
  // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) -- fixed seed for reproducibility
  std::mt19937_64 rng(0xC0FFEE);
  for (int i = 0; i < 100; i++) {
    uint64_t mac = rng() & 0xFFFFFFFFFFFFULL;
    verify_mac(mac, expected_varint_len(mac));
  }
}

}  // namespace esphome::api::testing
