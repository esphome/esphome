#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/proto.h"

namespace esphome::api::testing {

// The code generator writes an element of a repeated sint32 field with a single-byte tag as the tag byte followed by
// encode_varint_raw_short(); it must produce the same bytes as encode_sint32_force().
static std::vector<uint8_t> encode_both(int32_t value, bool raw) {
  APIBuffer buffer;
  EXPECT_TRUE(buffer.resize(16));
  uint8_t *pos = buffer.data();
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = buffer.data() + buffer.size();
#endif
  if (raw) {
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 24);
    pos = ProtoEncode::encode_varint_raw_short(pos PROTO_ENCODE_DEBUG_ARG, encode_zigzag32(value));
  } else {
    pos = ProtoEncode::encode_sint32_force(pos PROTO_ENCODE_DEBUG_ARG, 3, value);
  }
  return {buffer.data(), pos};
}

// Values whose zigzag form takes 1 to 5 bytes, either side of each boundary
TEST(ProtoRepeatedSint32, RawElementMatchesForcedEncode) {
  const int32_t values[] = {0,
                            1,
                            -1,
                            63,
                            -64,
                            64,
                            -65,
                            560,
                            -560,
                            8191,
                            -8192,
                            8192,
                            -8193,
                            9000,
                            -4500,
                            1048575,
                            -1048576,
                            1048576,
                            -1048577,
                            134217727,
                            -134217728,
                            134217728,
                            -134217729,
                            std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::min()};
  for (int32_t value : values) {
    const auto raw = encode_both(value, true);
    EXPECT_EQ(raw, encode_both(value, false)) << "value " << value;
    EXPECT_EQ(raw.size(), 1 + ProtoSize::varint(encode_zigzag32(value))) << "value " << value;
  }
}

}  // namespace esphome::api::testing
