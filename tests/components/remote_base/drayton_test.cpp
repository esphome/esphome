#include <gtest/gtest.h>
#include <string>
#include "esphome/components/remote_base/drayton_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr int32_t BIT_US = 500;
constexpr size_t PACKET_BITS = 28;

// Captured packets from the header comment of drayton_protocol.cpp
constexpr uint32_t ON_PACKET = 0x6180832;
constexpr uint32_t OFF_PACKET = 0x6180052;
const char *const ON_CAPTURE = "101010101010110001101001010101101001010101010101100101010101101001011001";
const char *const OFF_CAPTURE = "101010101010110001101001010101101001010101010101010101010110011001011001";

DraytonData with_data(uint16_t address, uint8_t data) {
  DraytonData out{};
  out.address = address;
  out.data = data;
  return out;
}

DraytonData with_legacy(uint16_t address, uint8_t channel, uint8_t command) {
  DraytonData out{};
  out.address = address;
  out.channel = channel;
  out.command = command;
  return out;
}

RawTimings encode(const DraytonData &data) {
  RemoteTransmitData tx;
  DraytonProtocol().encode(&tx, data);
  return tx.get_data();
}

// The packet bits sent after the preamble and sync, one Manchester pair per bit ('10' = 1)
uint32_t sent_packet(const RawTimings &timings) {
  uint32_t word = 0;
  for (size_t i = timings.size() - 2 * PACKET_BITS; i < timings.size(); i += 2) {
    word = (word << 1) | (timings[i] > 0 ? 1 : 0);
  }
  return word;
}

// A receiver reports runs of the same level as one item.
RawTimings as_received(const RawTimings &timings) {
  RawTimings merged;
  for (int32_t item : timings) {
    if (!merged.empty() && (merged.back() < 0) == (item < 0)) {
      merged.back() += item;
    } else {
      merged.push_back(item);
    }
  }
  return merged;
}

RawTimings from_capture(const std::string &bits) {
  RawTimings timings;
  for (char bit : bits) {
    timings.push_back(bit == '1' ? BIT_US : -BIT_US);
  }
  return as_received(timings);
}

optional<DraytonData> decode(const RawTimings &timings) {
  return DraytonProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(DraytonProtocolTest, EncodeAppendsChecksum) {
  EXPECT_EQ(sent_packet(encode(with_data(0x6180, 0x8))), ON_PACKET);
  EXPECT_EQ(sent_packet(encode(with_data(0x6180, 0x0))), OFF_PACKET);
  // A lambda value above 4 bits is masked instead of spilling into the address
  EXPECT_EQ(sent_packet(encode(with_data(0x6180, 0x18))), ON_PACKET);
}

TEST(DraytonProtocolTest, LegacyEncodeSendsChannelAndCommandAsIs) {
  EXPECT_EQ(sent_packet(encode(with_legacy(0x6180, 0x12, 0x41))), ON_PACKET);
  EXPECT_EQ(sent_packet(encode(with_legacy(0x1234, 0x01, 0x05))), 0x12340A1u);
}

TEST(DraytonProtocolTest, DecodesCapturedPackets) {
  auto on = decode(from_capture(ON_CAPTURE));
  ASSERT_TRUE(on.has_value());
  if (!on.has_value()) {
    return;
  }
  EXPECT_EQ(on->address, 0x6180);
  EXPECT_EQ(on->data, 0x8);
  EXPECT_EQ(on->channel, 0x12);
  EXPECT_EQ(on->command, 0x41);

  auto off = decode(from_capture(OFF_CAPTURE));
  ASSERT_TRUE(off.has_value());
  if (!off.has_value()) {
    return;
  }
  EXPECT_EQ(off->address, 0x6180);
  EXPECT_EQ(off->data, 0x0);
  EXPECT_EQ(off->channel, 0x12);
  EXPECT_EQ(off->command, 0x02);
}

TEST(DraytonProtocolTest, RoundTrips) {
  for (uint16_t address : {0x0000, 0x6180, 0x1231, 0xFFFF}) {
    for (uint8_t data : {0x0, 0x2, 0x8, 0xA, 0xF}) {
      auto decoded = decode(as_received(encode(with_data(address, data))));
      ASSERT_TRUE(decoded.has_value()) << std::hex << address << " " << int(data);
      if (!decoded.has_value()) {
        return;
      }
      EXPECT_EQ(decoded->address, address);
      EXPECT_EQ(decoded->data, data);
    }
  }
}

TEST(DraytonProtocolTest, RejectsBadChecksum) {
  // On packet with the low checksum bits changed from 0x12 to 0x13
  EXPECT_FALSE(decode(as_received(encode(with_legacy(0x6180, 0x13, 0x41)))).has_value());
  // On packet with the data bits of the Off packet
  EXPECT_FALSE(decode(as_received(encode(with_legacy(0x6180, 0x12, 0x01)))).has_value());
}

TEST(DraytonProtocolTest, BinarySensorFormsMatchDecodedPacket) {
  auto on = decode(from_capture(ON_CAPTURE));
  ASSERT_TRUE(on.has_value());
  if (!on.has_value()) {
    return;
  }
  // The decoded channel and command are ignored
  EXPECT_TRUE(*on == with_data(0x6180, 0x8));
  EXPECT_FALSE(*on == with_data(0x6181, 0x8));
}

}  // namespace esphome::remote_base::testing
