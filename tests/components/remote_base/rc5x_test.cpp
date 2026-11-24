#include <gtest/gtest.h>
#include "esphome/components/remote_base/rc5x_protocol.h"

namespace esphome::remote_base::testing {

namespace {

RawTimings encode(const RC5XData &data) {
  RemoteTransmitData tx;
  RC5XProtocol().encode(&tx, data);
  return tx.get_data();
}

// A receiver starts on the first mark, sees runs of one level as one item, and may drop a trailing space.
RawTimings as_received(const RawTimings &timings, bool drop_tail = false) {
  RawTimings merged;
  for (int32_t item : timings) {
    if (merged.empty() && item < 0) {
      continue;
    }
    if (!merged.empty() && (merged.back() < 0) == (item < 0)) {
      merged.back() += item;
    } else {
      merged.push_back(item);
    }
  }
  if (drop_tail && !merged.empty() && merged.back() < 0) {
    merged.pop_back();
  }
  return merged;
}

RawTimings inverted(RawTimings timings) {
  for (int32_t &item : timings) {
    item = -item;
  }
  return timings;
}

optional<RC5XData> decode(const RawTimings &timings) {
  return RC5XProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

void expect_round_trip(const RC5XData &sent) {
  // encode() flips the toggle on every call, so two calls cover both toggle values
  const RawTimings first = encode(sent);
  const RawTimings second = encode(sent);
  ASSERT_NE(first, second);
  for (const RawTimings &timings : {first, second}) {
    for (bool drop_tail : {false, true}) {
      auto decoded = decode(as_received(timings, drop_tail));
      ASSERT_TRUE(decoded.has_value()) << std::hex << "address 0x" << int(sent.address) << " command 0x"
                                       << int(sent.command) << " extension 0x" << int(sent.extension);
      if (!decoded.has_value()) {
        return;
      }
      EXPECT_EQ(*decoded, sent);
    }
  }
}

}  // namespace

TEST(RC5XProtocolTest, RoundTripsBothToggleValues) {
  expect_round_trip({.address = 0x10, .command = 0x00, .extension = 0x06});
  expect_round_trip({.address = 0x1F, .command = 0x0D, .extension = 0x15});
}

TEST(RC5XProtocolTest, RoundTripsCommandsWithBit5Set) {
  expect_round_trip({.address = 0x10, .command = 0x20, .extension = 0x00});
  expect_round_trip({.address = 0x01, .command = 0x3F, .extension = 0x2A});
  expect_round_trip({.address = 0x00, .command = 0x25, .extension = 0x3F});
}

TEST(RC5XProtocolTest, RoundTripsExtendedCommands) {
  expect_round_trip({.address = 0x10, .command = 0x40, .extension = 0x06});
  expect_round_trip({.address = 0x0A, .command = 0x55, .extension = 0x00});
  expect_round_trip({.address = 0x15, .command = 0x7F, .extension = 0x3F});
  expect_round_trip({.address = 0x10, .command = 0x60, .extension = 0x01});
}

TEST(RC5XProtocolTest, RoundTripsExtensionLimits) {
  for (uint8_t address : {0x00, 0x1F}) {
    for (uint8_t command : {0x00, 0x1F, 0x20, 0x3F, 0x40, 0x7F}) {
      expect_round_trip({.address = address, .command = command, .extension = 0x00});
      expect_round_trip({.address = address, .command = command, .extension = 0x3F});
    }
  }
}

TEST(RC5XProtocolTest, DecodesCapturedMarantzAux1) {
  // Captured AUX1 code (address 0x10, command 0x00, extension 0x06, toggle set) from the PR description
  const RawTimings captured = {889, -889,  889, -889, 889, -889, 1778, -889,  889, -889, 889, -889,
                               889, -4450, 889, -889, 889, -889, 889,  -889,  889, -889, 889, -889,
                               889, -889,  889, -889, 889, -889, 889,  -1778, 889, -889, 1778};
  auto decoded = decode(captured);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->address, 0x10);
  EXPECT_EQ(decoded->command, 0x00);
  EXPECT_EQ(decoded->extension, 0x06);
}

TEST(RC5XProtocolTest, EncodeMatchesCapturedMarantzAux1) {
  const RawTimings captured = {889, -889,  889, -889, 889, -889, 1778, -889,  889, -889, 889, -889,
                               889, -4450, 889, -889, 889, -889, 889,  -889,  889, -889, 889, -889,
                               889, -889,  889, -889, 889, -889, 889,  -1778, 889, -889, 1778};
  const RC5XData aux1{.address = 0x10, .command = 0x00, .extension = 0x06};
  // The capture has the toggle set; one of two consecutive frames does too
  const RawTimings first = as_received(encode(aux1), true);
  const RawTimings second = as_received(encode(aux1), true);
  const RawTimings &toggled = first[6] == 1778 ? first : second;
  ASSERT_EQ(toggled.size(), captured.size());
  for (size_t i = 0; i < captured.size(); i++) {
    // The pause is 4 x 889 plus the half-bit before it, sent as 4445 against 4450 captured
    EXPECT_NEAR(toggled[i], captured[i], 5) << i;
  }
}

TEST(RC5XProtocolTest, DecodesAtInvertedPolarity) {
  const RC5XData sent{.address = 0x10, .command = 0x6A, .extension = 0x2B};
  // With the pin inverted, every carrier half reads as a space
  const RawTimings timings = inverted(as_received(encode(sent), true));
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, sent);
}

TEST(RC5XProtocolTest, MasksOutOfRangeFields) {
  // Bit 7 of the command has no place on the wire and is dropped like the other high bits
  RC5XData sent{.address = 0xF0, .command = 0xC5, .extension = 0xC6};
  auto decoded = decode(as_received(encode(sent)));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->address, 0x10);
  EXPECT_EQ(decoded->command, 0x45);
  EXPECT_EQ(decoded->extension, 0x06);
}

TEST(RC5XProtocolTest, RejectsPlainRc5AndBrokenFrames) {
  // Without the pause the frame is too short
  RawTimings timings = as_received(encode({.address = 0x10, .command = 0x00, .extension = 0x06}));
  RawTimings no_pause;
  for (int32_t item : timings) {
    no_pause.push_back(item < -3000 ? -889 : item);
  }
  EXPECT_FALSE(decode(no_pause).has_value());
  // A pause far too long
  for (int32_t &item : timings) {
    if (item < -3000) {
      item = -9000;
    }
  }
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
