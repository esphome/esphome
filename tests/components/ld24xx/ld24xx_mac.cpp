#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include "esphome/components/ld24xx/ld24xx.h"

namespace esphome::ld24xx::testing {

static std::string format_mac(const uint8_t (&mac)[MAC_ADDRESS_SIZE]) {
  char buffer[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  return format_mac_str(mac, buffer);
}

TEST(LD24xxFormatMac, MulticastBitMacIsShownAsIs) {
  // First byte 0x81 has the multicast bit set, and the radar reports it as its Bluetooth MAC.
  const uint8_t mac[MAC_ADDRESS_SIZE] = {0x81, 0x2C, 0x62, 0xBE, 0xC9, 0x96};
  EXPECT_EQ(format_mac(mac), "81:2C:62:BE:C9:96");
}

TEST(LD24xxFormatMac, UnicastMacIsShownAsIs) {
  const uint8_t mac[MAC_ADDRESS_SIZE] = {0x80, 0x2C, 0x62, 0xBE, 0xC9, 0x96};
  EXPECT_EQ(format_mac(mac), "80:2C:62:BE:C9:96");
}

TEST(LD24xxFormatMac, AllZeroMacIsUnknown) {
  const uint8_t mac[MAC_ADDRESS_SIZE] = {0, 0, 0, 0, 0, 0};
  EXPECT_EQ(format_mac(mac), "unknown");
}

TEST(LD24xxFormatMac, AllFfMacIsUnknown) {
  const uint8_t mac[MAC_ADDRESS_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(format_mac(mac), "unknown");
}

TEST(LD24xxFormatMac, MacWithSomeZeroOrFfBytesIsShown) {
  const uint8_t mac[MAC_ADDRESS_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
  EXPECT_EQ(format_mac(mac), "00:00:00:00:00:01");
  const uint8_t ff_mac[MAC_ADDRESS_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
  EXPECT_EQ(format_mac(ff_mac), "FF:FF:FF:FF:FF:FE");
}

}  // namespace esphome::ld24xx::testing
