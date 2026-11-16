#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/haier_protocol.h"
#include "esphome/core/alloc_helpers.h"

namespace esphome::remote_base::testing {

TEST(RemoteBytesTest, KeepsTheBytesAndConvertsToAVector) {
  RemoteBytes<4> bytes{0x12, 0x34, 0x56};
  ASSERT_EQ(bytes.size(), 3u);
  EXPECT_EQ(bytes.front(), 0x12);
  EXPECT_EQ(bytes.back(), 0x56);
  std::vector<uint8_t> as_vector = bytes;
  EXPECT_EQ(as_vector, (std::vector<uint8_t>{0x12, 0x34, 0x56}));
  // Lambdas pass x.data to helpers that take a std::vector
  EXPECT_EQ(format_hex(bytes), "123456");
}

TEST(RemoteBytesTest, BuildsFromAVectorAndDropsBytesPastCapacity) {
  RemoteBytes<2> bytes = std::vector<uint8_t>{1, 2, 3};
  EXPECT_EQ(bytes.size(), 2u);
  EXPECT_EQ(bytes, (RemoteBytes<2>{1, 2}));
}

TEST(RemoteBytesTest, ComparesSizeAndContent) {
  EXPECT_EQ((RemoteBytes<4>{1, 2}), (RemoteBytes<4>{1, 2}));
  EXPECT_NE((RemoteBytes<4>{1, 2}), (RemoteBytes<4>{1, 2, 3}));
  EXPECT_NE((RemoteBytes<4>{1, 2}), (RemoteBytes<4>{1, 3}));
}

TEST(RemoteBytesTest, ComparesWithAVectorWithoutCuttingIt) {
  const std::vector<uint8_t> longer{1, 2, 3};
  EXPECT_FALSE((RemoteBytes<2>{1, 2} == longer));
  EXPECT_FALSE((longer == RemoteBytes<2>{1, 2}));
  EXPECT_TRUE((RemoteBytes<2>{1, 2} == std::vector<uint8_t>{1, 2}));
  EXPECT_TRUE((std::vector<uint8_t>{1, 2} == RemoteBytes<2>{1, 2}));
}

template<typename P> typename P::ProtocolData round_trip(const typename P::ProtocolData &data) {
  RemoteTransmitData tx;
  P().encode(&tx, data);
  RawTimings timings = tx.get_data();
  // Haier needs an item after its last mark; the others ignore it
  timings.push_back(-10000);
  auto decoded = P().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
  EXPECT_TRUE(decoded.has_value());
  return decoded.value_or(typename P::ProtocolData{});
}

// The long Haier frame and the Mirage frame are covered by ir_code_encoders_test
TEST(RemoteBytesTest, HaierShortFrameRoundTrips) {
  HaierData short_frame{{0xA5, 0x12, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00}};
  EXPECT_EQ(round_trip<HaierProtocol>(short_frame), short_frame);
}

TEST(RemoteBytesTest, AehaFrameOfEveryLengthRoundTrips) {
  for (size_t len = 2; len <= AEHA_MAX_DATA_BYTES; len++) {
    AEHAData frame{.address = 0x2002, .data = {}};
    for (size_t i = 0; i < len; i++)
      frame.data.push_back(static_cast<uint8_t>(i * 37 + 1));
    EXPECT_EQ(round_trip<AEHAProtocol>(frame), frame) << "length " << len;
  }
}

}  // namespace esphome::remote_base::testing
