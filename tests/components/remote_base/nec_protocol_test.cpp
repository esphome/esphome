#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/remote_base/nec_protocol.h"

namespace esphome::remote_base::testing {

TEST(NECProtocolTest, EncodeWithoutRepeats) {
  NECProtocol protocol;
  NECData data{
      .address = 0x7F80,
      .command = 0xF20D,
      .command_repeats = 1,
  };
  RemoteTransmitData dst;
  protocol.encode(&dst, data);

  // We won't verify the exact bits of address and command,
  // but we will verify the structure (header, bits, final mark).
  auto data_vec = dst.get_data();
  // Header: 9000 mark, 4500 space
  EXPECT_EQ(data_vec[0], 9000);
  EXPECT_EQ(data_vec[1], -4500);

  // Address (16 bits) + Command (16 bits) = 32 bits
  // Each bit is 2 items (mark + space)
  // Total items = 2 (header) + 32 * 2 + 1 (final mark) = 67 items
  EXPECT_EQ(data_vec.size(), 67);

  // Final mark
  EXPECT_EQ(data_vec[66], 560);
}

TEST(NECProtocolTest, EncodeWithRepeats) {
  NECProtocol protocol;
  NECData data{
      .address = 0x7F80,
      .command = 0xF20D,
      .command_repeats = 3,
  };
  RemoteTransmitData dst;
  protocol.encode(&dst, data);

  auto data_vec = dst.get_data();

  // First 67 items are the initial transmission (Header + 32 bits + final mark)
  // Then space(40500) -> 1 item
  // Then repeat code 1: item(9000, 2250), mark(560) -> 3 items
  // Then space(96187) -> 1 item
  // Then repeat code 2: item(9000, 2250), mark(560) -> 3 items
  // Total = 67 + 1 + 3 + 1 + 3 = 75 items

  EXPECT_EQ(data_vec.size(), 75);

  // Verify repeat structure
  EXPECT_EQ(data_vec[67], -40500);  // 40.5ms space
  EXPECT_EQ(data_vec[68], 9000);    // Repeat header high
  EXPECT_EQ(data_vec[69], -2250);   // Repeat header low
  EXPECT_EQ(data_vec[70], 560);     // Repeat mark

  EXPECT_EQ(data_vec[71], -96187);  // 96.1875ms space
  EXPECT_EQ(data_vec[72], 9000);    // Repeat header high
  EXPECT_EQ(data_vec[73], -2250);   // Repeat header low
  EXPECT_EQ(data_vec[74], 560);     // Repeat mark
}

TEST(NECProtocolTest, DecodeWithoutRepeats) {
  NECProtocol protocol;
  NECData encode_data{
      .address = 0x1234,
      .command = 0x5678,
      .command_repeats = 1,
  };
  RemoteTransmitData tx;
  protocol.encode(&tx, encode_data);

  RemoteReceiveData rx(tx.get_data(), 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->address, 0x1234);
    EXPECT_EQ(decoded->command, 0x5678);
    EXPECT_EQ(decoded->command_repeats, 1);
  }
}

TEST(NECProtocolTest, DecodeWithRepeats) {
  NECProtocol protocol;
  NECData encode_data{
      .address = 0x1234,
      .command = 0x5678,
      .command_repeats = 4,
  };
  RemoteTransmitData tx;
  protocol.encode(&tx, encode_data);

  RemoteReceiveData rx(tx.get_data(), 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->address, 0x1234);
    EXPECT_EQ(decoded->command, 0x5678);
    EXPECT_EQ(decoded->command_repeats, 4);
  }
}

}  // namespace esphome::remote_base::testing
