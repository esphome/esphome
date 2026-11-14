#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "esphome/components/brizlabs_led_string/encoder.h"

namespace esphome::brizlabs_led_string::testing {

TEST(BrizLabsEncoder, KnownFrame) {
  uint8_t out[FRAME_BYTES];
  memset(out, 0xFF, sizeof(out));  // a reused buffer must come back zero padded
  // Slots, with 0 = "10" and 1 = "110", each field LSB first:
  //   id 0        101010101010
  //   zone 1      110101010
  //   header FF   110 x 8
  //   R FF        110 x 8
  //   G 00        10 x 8
  //   B 80        10 x 7, then 110
  //   trailer     11110000
  EXPECT_EQ(encode(out, 0, 1, 0xFF, 0xFF, 0x00, 0x80), 110u);
  EXPECT_EQ(std::vector<uint8_t>(out, out + FRAME_BYTES),
            (std::vector<uint8_t>{0xAA, 0xAD, 0x56, 0xDB, 0x6D, 0xB6, 0xDB, 0x6D, 0xB5, 0x55, 0x55, 0x55, 0x5B, 0xC0,
                                  0x00, 0x00, 0x00}));
}

TEST(BrizLabsEncoder, FrameLength) {
  uint8_t out[FRAME_BYTES];
  EXPECT_EQ(encode(out, 0, 0, 0, 0, 0, 0), 42u * 2 + 8);
  EXPECT_EQ(encode(out, 0x3F, 0xF, 0xFF, 0xFF, 0xFF, 0xFF), 42u * 3 + 8);
}

}  // namespace esphome::brizlabs_led_string::testing
