#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/proto.h"

namespace esphome::api::testing {

// Encodes into a buffer of exactly the calculated size, so any overrun trips the debug bounds check.
static std::vector<uint8_t> encode(const UsbDeviceDescriptor &msg) {
  const uint32_t size = msg.calculate_size();
  APIBuffer buf;
  EXPECT_TRUE(buf.resize(size == 0 ? 1 : size));
  uint8_t *pos = buf.data();
#ifdef ESPHOME_DEBUG_API
  uint8_t *proto_debug_end_ = buf.data() + buf.size();
#endif
  uint8_t *end = UsbDeviceDescriptor::encode_msg(&msg, pos PROTO_ENCODE_DEBUG_ARG);
  EXPECT_EQ(static_cast<uint32_t>(end - buf.data()), size);
  return {buf.data(), end};
}

TEST(UsbDeviceDescriptor, EncodesTheInterfaceClassAndPort) {
  UsbDeviceDescriptor msg;
  msg.vendor_id = 0x10C4;
  msg.product_id = 0xEA60;
  msg.bcd_device = 0x0100;
  msg.interface_number = 1;
  msg.interface_class = 0xFF;
  msg.port_number = 2;

  const std::vector<uint8_t> expected = {0x08, 0xC4, 0x21, 0x10, 0xE0, 0xD4, 0x03, 0x18, 0x80,
                                         0x02, 0x20, 0x01, 0x28, 0xFF, 0x01, 0x30, 0x02};
  EXPECT_EQ(encode(msg), expected);
}

TEST(UsbDeviceDescriptor, ZeroClassAndPortLeaveTheWireAsBefore) {
  // The new fields are additive with zero defaults, so an old client sees the message it always did
  UsbDeviceDescriptor msg;
  msg.vendor_id = 0x303A;
  msg.product_id = 0x1001;
  msg.interface_number = 1;

  const std::vector<uint8_t> expected = {0x08, 0xBA, 0x60, 0x10, 0x81, 0x20, 0x20, 0x01};
  EXPECT_EQ(encode(msg), expected);
}

}  // namespace esphome::api::testing
