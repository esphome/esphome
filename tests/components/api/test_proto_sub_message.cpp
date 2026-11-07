#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/proto.h"

namespace esphome::api::testing {

// Sub-message whose body is raw bytes, so the body size is set directly by the test.
struct BlobMessage {
  const uint8_t *data;
  uint32_t len;
  static uint8_t *encode_msg(const void *self, uint8_t *pos PROTO_ENCODE_DEBUG_PARAM) {
    const auto &msg = *static_cast<const BlobMessage *>(self);
    // An empty vector's data() may be null, and memcpy needs a valid source even for zero bytes
    if (msg.len == 0)
      return pos;
    return ProtoEncode::encode_raw(pos PROTO_ENCODE_DEBUG_ARG, msg.data, msg.len);
  }
  static uint32_t calc_size_msg(const void *self) { return static_cast<const BlobMessage *>(self)->len; }
};

static void append_varint(std::vector<uint8_t> &out, uint32_t value) {
  while (value > 0x7F) {
    out.push_back(static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<uint8_t>(value));
}

static std::vector<uint8_t> make_body(uint32_t len) {
  std::vector<uint8_t> body(len);
  for (uint32_t i = 0; i < len; i++)
    body[i] = static_cast<uint8_t>(i * 7 + 1);
  return body;
}

static std::vector<uint8_t> expected_field(uint32_t field_id, const std::vector<uint8_t> &body) {
  std::vector<uint8_t> out;
  append_varint(out, (field_id << 3) | 2);
  append_varint(out, body.size());
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

// Encodes into a buffer of exactly the expected size, so any overrun trips ASan or the debug bounds check.
template<bool OPTIONAL> static void verify(uint32_t field_id, uint32_t body_len) {
  std::vector<uint8_t> body = make_body(body_len);
  std::vector<uint8_t> expected = expected_field(field_id, body);
  if (OPTIONAL && body_len == 0)
    expected.clear();
  BlobMessage msg{body.data(), body_len};

  APIBuffer buf;
  ASSERT_TRUE(buf.resize(expected.empty() ? 1 : expected.size()));
  uint8_t *pos = buf.data();
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = buf.data() + buf.size();
#endif
  uint8_t *end;
  if constexpr (OPTIONAL) {
    end = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, field_id, msg);
  } else {
    end = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, field_id, msg);
  }

  ASSERT_EQ(static_cast<size_t>(end - buf.data()), expected.size()) << "field " << field_id << " body " << body_len;
  EXPECT_EQ(std::vector<uint8_t>(buf.data(), end), expected) << "field " << field_id << " body " << body_len;
}

TEST(ProtoSubMessage, OneByteTag) { verify<false>(4, 10); }
TEST(ProtoSubMessage, TwoByteTag) { verify<false>(20, 10); }
TEST(ProtoSubMessage, EmptyBody) { verify<false>(20, 0); }
TEST(ProtoSubMessage, LongestOneByteLength) { verify<false>(20, 127); }
// The length outgrows its reserved byte, so the body is moved forward
TEST(ProtoSubMessage, TwoByteLength) {
  verify<false>(20, 128);
  verify<false>(25, 200);
}
TEST(ProtoSubMessage, ThreeByteLength) { verify<false>(4, 20000); }

TEST(ProtoOptionalSubMessage, EmptyIsSkipped) { verify<true>(22, 0); }
TEST(ProtoOptionalSubMessage, OneByteTag) { verify<true>(1, 10); }
TEST(ProtoOptionalSubMessage, TwoByteTagAndLength) { verify<true>(22, 200); }

}  // namespace esphome::api::testing
