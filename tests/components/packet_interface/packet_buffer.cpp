#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include "esphome/components/packet_interface/packet_buffer.h"

namespace esphome {
namespace packet_interface {
namespace testing {

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

// Test to_vector method
TEST(PacketBufferTest, ToVector) {
  uint8_t data[] = {0xAA, 0xBB, 0xCC};
  PacketBuffer buffer(data, 3);

  std::vector<uint8_t> vec = buffer.to_vector();

  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 0xAA);
  EXPECT_EQ(vec[1], 0xBB);
  EXPECT_EQ(vec[2], 0xCC);
}

// Test from_vector static method
TEST(PacketBufferTest, FromVector) {
  std::vector<uint8_t> vec = {0x11, 0x22, 0x33};
  PacketBuffer buffer = PacketBuffer::from_vector(vec);

  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[0], 0x11);
  EXPECT_EQ(buffer[1], 0x22);
  EXPECT_EQ(buffer[2], 0x33);
}

// Test assignment operator from vector
TEST(PacketBufferTest, AssignmentFromVector) {
  std::vector<uint8_t> vec = {0x77, 0x88, 0x99};
  PacketBuffer buffer;

  buffer = vec;

  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[0], 0x77);
  EXPECT_EQ(buffer[1], 0x88);
  EXPECT_EQ(buffer[2], 0x99);
}

// Test conversion to vector
TEST(PacketBufferTest, ConversionToVector) {
  uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
  PacketBuffer buffer(data, 4);

  std::vector<uint8_t> vec = buffer;  // Implicit conversion

  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 0x10);
  EXPECT_EQ(vec[1], 0x20);
  EXPECT_EQ(vec[2], 0x30);
  EXPECT_EQ(vec[3], 0x40);
}

// Test iterator begin and end
TEST(PacketBufferTest, Iterator) {
  uint8_t data[] = {0x01, 0x02, 0x03};
  PacketBuffer buffer(data, 3);

  auto it = buffer.begin();
  EXPECT_EQ(*it, 0x01);
  ++it;
  EXPECT_EQ(*it, 0x02);
  ++it;
  EXPECT_EQ(*it, 0x03);
  ++it;
  EXPECT_EQ(it, buffer.end());
}

// Test range-based for loop
TEST(PacketBufferTest, RangeBasedFor) {
  uint8_t data[] = {0x10, 0x20, 0x30};
  PacketBuffer buffer(data, 3);

  std::vector<uint8_t> collected;
  for (uint8_t byte : buffer) {
    collected.push_back(byte);
  }

  EXPECT_EQ(collected.size(), 3);
  EXPECT_EQ(collected[0], 0x10);
  EXPECT_EQ(collected[1], 0x20);
  EXPECT_EQ(collected[2], 0x30);
}

// Test set_pre_buffer
TEST(PacketBufferTest, SetPreBuffer) {
  PacketBuffer buffer;
  uint8_t data[] = {0xAA, 0xBB};

  buffer.set_pre_buffer(data, 2);

  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], 0xAA);
  EXPECT_EQ(buffer[1], 0xBB);
}

// Test set_mid_buffer
TEST(PacketBufferTest, SetMidBuffer) {
  uint8_t data1[] = {0x01};
  uint8_t data2[] = {0x02, 0x03};
  uint8_t data3[] = {0x04};

  PacketBuffer mid(data2, 2);
  PacketBuffer buffer;

  buffer.set_pre_buffer(data1, 1);
  buffer.set_mid_buffer(&mid);
  buffer.set_post_buffer(data3, 1);

  EXPECT_EQ(buffer.size(), 4);
  EXPECT_EQ(buffer[0], 0x01);
  EXPECT_EQ(buffer[1], 0x02);
  EXPECT_EQ(buffer[2], 0x03);
  EXPECT_EQ(buffer[3], 0x04);
}

// Test set_post_buffer
TEST(PacketBufferTest, SetPostBuffer) {
  uint8_t data[] = {0xCC, 0xDD};
  PacketBuffer buffer;

  buffer.set_post_buffer(data, 2);

  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], 0xCC);
  EXPECT_EQ(buffer[1], 0xDD);
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

  std::vector<uint8_t> vec = buffer.to_vector();
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

}  // namespace testing
}  // namespace packet_interface
}  // namespace esphome
