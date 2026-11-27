#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include "esphome/components/packet_interface/packet_buffer.h"

namespace esphome::packet_interface::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

// Test default constructor
TEST(PacketBufferTest, DefaultConstructor) {
  PacketBuffer buffer;
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_TRUE(buffer.empty());
}

// Test single buffer constructor
TEST(PacketBufferTest, SingleBufferConstructor) {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  PacketBuffer buffer(data, 4);
  EXPECT_EQ(buffer.size(), 4);
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[3], 0x04);
}

// Test vector constructor
TEST(PacketBufferTest, VectorConstructor) {
  std::vector<uint8_t> vec = {0x10, 0x20, 0x30};
  PacketBuffer buffer(vec);
  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[0], 0x10);
  EXPECT_EQ(buffer[1], 0x20);
  EXPECT_EQ(buffer[2], 0x30);
}

// Test pre and post buffer constructor
TEST(PacketBufferTest, PrePostConstructor) {
  uint8_t pre[] = {0x01, 0x02};
  uint8_t post[] = {0x03, 0x04, 0x05};
  PacketBuffer buffer(pre, 2, post, 3);
  EXPECT_EQ(buffer.size(), 5);
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_EQ(buffer[3], 0x04);
  EXPECT_EQ(buffer[4], 0x05);
}

// Test pre, mid, and post buffer constructor
TEST(PacketBufferTest, PreMidPostConstructor) {
  uint8_t pre[] = {0x01};
  std::vector<uint8_t> mid_vec = {0x02, 0x03, 0x04};
  PacketBuffer mid_buffer(mid_vec);
  uint8_t post[] = {0x05, 0x06};

  PacketBuffer buffer(pre, 1, &mid_buffer, post, 2);
  EXPECT_EQ(buffer.size(), 6);
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_EQ(buffer[3], 0x04);
  EXPECT_EQ(buffer[4], 0x05);
  EXPECT_EQ(buffer[5], 0x06);
}

// Test nested mid buffers
TEST(PacketBufferTest, NestedMidBuffers) {
  uint8_t data1[] = {0x01};
  uint8_t data2[] = {0x02, 0x03};
  uint8_t data3[] = {0x04};

  PacketBuffer inner(data2, 2);
  PacketBuffer middle(data1, 1, &inner, data3, 1);

  uint8_t pre[] = {0x00};
  uint8_t post[] = {0x05};
  PacketBuffer outer(pre, 1, &middle, post, 1);

  EXPECT_EQ(outer.size(), 6);
  EXPECT_EQ(outer[0], 0x00);
  EXPECT_EQ(outer[1], 0x01);
  EXPECT_EQ(outer[2], 0x02);
  EXPECT_EQ(outer[3], 0x03);
  EXPECT_EQ(outer[4], 0x04);
  EXPECT_EQ(outer[5], 0x05);
}

// Test indexing operator
TEST(PacketBufferTest, IndexingOperator) {
  uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
  PacketBuffer buffer(data, 4);
  EXPECT_EQ(buffer[0], 0xAA);
  EXPECT_EQ(buffer[1], 0xBB);
  EXPECT_EQ(buffer[2], 0xCC);
  EXPECT_EQ(buffer[3], 0xDD);
}

// Test copy_to method
TEST(PacketBufferTest, CopyTo) {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  PacketBuffer buffer(data, 5);

  uint8_t dest[5] = {0};
  size_t copied = buffer.copy_to(dest, 5);

  EXPECT_EQ(copied, 5);
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(dest[i], data[i]);
  }
}

// Test copy_to with offset
TEST(PacketBufferTest, CopyToWithOffset) {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  PacketBuffer buffer(data, 5);

  uint8_t dest[3] = {0};
  size_t copied = buffer.copy_to(dest, 3, 2);  // Copy 3 bytes starting from offset 2

  EXPECT_EQ(copied, 3);
  EXPECT_EQ(dest[0], 0x03);
  EXPECT_EQ(dest[1], 0x04);
  EXPECT_EQ(dest[2], 0x05);
}

// Test copy_to with partial copy
TEST(PacketBufferTest, CopyToPartial) {
  uint8_t data[] = {0x01, 0x02, 0x03};
  PacketBuffer buffer(data, 3);

  uint8_t dest[5] = {0};
  size_t copied = buffer.copy_to(dest, 5);  // Request more than available

  EXPECT_EQ(copied, 3);  // Only 3 bytes copied
}

// Test copy_to from composite buffer
TEST(PacketBufferTest, CopyToComposite) {
  uint8_t pre[] = {0x01, 0x02};
  uint8_t mid[] = {0x03, 0x04, 0x05};
  uint8_t post[] = {0x06};

  PacketBuffer mid_buffer(mid, 3);
  PacketBuffer buffer(pre, 2, &mid_buffer, post, 1);

  uint8_t dest[6] = {0};
  size_t copied = buffer.copy_to(dest, 6);

  EXPECT_EQ(copied, 6);
  EXPECT_EQ(dest[0], 0x01);
  EXPECT_EQ(dest[1], 0x02);
  EXPECT_EQ(dest[2], 0x03);
  EXPECT_EQ(dest[3], 0x04);
  EXPECT_EQ(dest[4], 0x05);
  EXPECT_EQ(dest[5], 0x06);
}

// Test copying from an offset past the mid buffer
TEST(PacketBufferTest, CopyToOffsetPastMid) {
  uint8_t pre[] = {0x01, 0x02};
  uint8_t mid_data[] = {0x03};
  uint8_t post[] = {0x04, 0x05, 0x06};
  PacketBuffer mid(mid_data, 1);
  PacketBuffer buffer(pre, 2, &mid, post, 3);

  uint8_t dest[4]{};
  ASSERT_EQ(buffer.copy_to(dest, 4, 4), 2);
  EXPECT_EQ(dest[0], 0x05);
  EXPECT_EQ(dest[1], 0x06);
}

// Test single span access
TEST(PacketBufferTest, ContiguousSpan) {
  uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
  PacketBuffer buffer(data, 4);

  ASSERT_TRUE(buffer.is_contiguous());
  auto span = buffer.as_span();
  EXPECT_EQ(span.data(), data);
  EXPECT_EQ(span.size(), 4);

  uint8_t post[] = {0x50};
  PacketBuffer split(data, 4, post, 1);
  EXPECT_FALSE(split.is_contiguous());

  PacketBuffer nested(nullptr, 0, &buffer, nullptr, 0);
  EXPECT_FALSE(nested.is_contiguous());
}

// Test indexing past the end
TEST(PacketBufferTest, IndexPastEnd) {
  uint8_t pre[] = {0x01};
  uint8_t post[] = {0x02};
  PacketBuffer buffer(pre, 1, post, 1);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0);
  EXPECT_EQ(PacketBuffer()[0], 0);
}

// Test empty buffer operations
TEST(PacketBufferTest, EmptyBuffer) {
  PacketBuffer buffer;

  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);

  uint8_t dest[10] = {0xFF};
  size_t copied = buffer.copy_to(dest, 10);
  EXPECT_EQ(copied, 0);
  EXPECT_EQ(dest[0], 0xFF);  // Unchanged

  std::vector<uint8_t> vec = to_vector(buffer);
  EXPECT_EQ(vec.size(), 0);
}

// Test large buffer
TEST(PacketBufferTest, LargeBuffer) {
  std::vector<uint8_t> large_vec(1000);
  for (size_t i = 0; i < large_vec.size(); i++) {
    large_vec[i] = static_cast<uint8_t>(i % 256);
  }

  PacketBuffer buffer(large_vec);

  EXPECT_EQ(buffer.size(), 1000);
  EXPECT_EQ(buffer[0], 0);
  EXPECT_EQ(buffer[255], 255);
  EXPECT_EQ(buffer[256], 0);
  EXPECT_EQ(buffer[999], 231);  // 999 % 256 = 231
}

}  // namespace esphome::packet_interface::testing
