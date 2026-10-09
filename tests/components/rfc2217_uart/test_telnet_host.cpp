#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/rfc2217_uart/telnet.h"

#ifdef USE_HOST

namespace esphome::rfc2217_uart::testing {
namespace {

using Event = TelnetDecoder::Event;

TEST(Rfc2217Telnet, DoubledIacIsOnePayloadByte) {
  TelnetDecoder decoder;
  EXPECT_EQ(decoder.feed(0x41), Event::DATA);
  EXPECT_EQ(decoder.data(), 0x41);
  EXPECT_EQ(decoder.feed(TELNET_IAC), Event::NONE);
  EXPECT_FALSE(decoder.idle());
  EXPECT_EQ(decoder.feed(TELNET_IAC), Event::DATA);
  EXPECT_EQ(decoder.data(), TELNET_IAC);
  EXPECT_TRUE(decoder.idle());
}

TEST(Rfc2217Telnet, XonAndXoffArePayload) {
  TelnetDecoder decoder;
  EXPECT_EQ(decoder.feed(0x11), Event::DATA);
  EXPECT_EQ(decoder.data(), 0x11);
  EXPECT_EQ(decoder.feed(0x13), Event::DATA);
  EXPECT_EQ(decoder.data(), 0x13);
}

TEST(Rfc2217Telnet, OptionCommand) {
  TelnetDecoder decoder;
  EXPECT_EQ(decoder.feed(TELNET_IAC), Event::NONE);
  EXPECT_EQ(decoder.feed(TELNET_WILL), Event::NONE);
  EXPECT_EQ(decoder.feed(OPTION_COM_PORT), Event::OPTION);
  EXPECT_EQ(decoder.verb(), TELNET_WILL);
  EXPECT_EQ(decoder.option(), OPTION_COM_PORT);
  EXPECT_TRUE(decoder.idle());
}

TEST(Rfc2217Telnet, SubnegotiationUndoesDoubledIac) {
  TelnetDecoder decoder;
  const uint8_t stream[] = {TELNET_IAC, TELNET_SB, OPTION_COM_PORT, COM_SET_BAUDRATE, 0x00, 0x00, 0xFF,
                            0xFF,       0x00,      TELNET_IAC};
  for (uint8_t byte : stream) {
    EXPECT_EQ(decoder.feed(byte), Event::NONE);
  }
  ASSERT_EQ(decoder.feed(TELNET_SE), Event::SUBNEGOTIATION);
  const uint8_t want[] = {OPTION_COM_PORT, COM_SET_BAUDRATE, 0x00, 0x00, 0xFF, 0x00};
  ASSERT_EQ(decoder.sub_len(), sizeof(want));
  for (size_t i = 0; i < sizeof(want); i++) {
    EXPECT_EQ(decoder.sub()[i], want[i]);
  }
}

TEST(Rfc2217Telnet, OtherCommandEndsASubnegotiation) {
  TelnetDecoder decoder;
  const uint8_t stream[] = {TELNET_IAC, TELNET_SB, OPTION_COM_PORT, TELNET_IAC, TELNET_WILL};
  for (uint8_t byte : stream) {
    EXPECT_EQ(decoder.feed(byte), Event::NONE);
  }
  EXPECT_EQ(decoder.feed(OPTION_BINARY), Event::OPTION);
  EXPECT_EQ(decoder.verb(), TELNET_WILL);
  EXPECT_EQ(decoder.feed(0x41), Event::DATA);
}

TEST(Rfc2217Telnet, OverlongSubnegotiationIsSkipped) {
  TelnetDecoder decoder;
  decoder.feed(TELNET_IAC);
  decoder.feed(TELNET_SB);
  for (size_t i = 0; i < TelnetDecoder::SUB_SIZE + 4; i++) {
    EXPECT_EQ(decoder.feed('a'), Event::NONE);
  }
  decoder.feed(TELNET_IAC);
  EXPECT_EQ(decoder.feed(TELNET_SE), Event::NONE);
  EXPECT_EQ(decoder.feed(0x42), Event::DATA);
}

TEST(Rfc2217Telnet, TwoByteCommandCarriesNothing) {
  TelnetDecoder decoder;
  decoder.feed(TELNET_IAC);
  // NOP
  EXPECT_EQ(decoder.feed(241), Event::NONE);
  EXPECT_TRUE(decoder.idle());
  decoder.feed(TELNET_IAC);
  decoder.reset();
  EXPECT_TRUE(decoder.idle());
}

TEST(Rfc2217Telnet, EscapeDoublesIacAndNeverSplitsAPair) {
  const uint8_t src[] = {0x01, TELNET_IAC, 0x02};
  uint8_t dst[8];
  size_t used = 0;
  EXPECT_EQ(telnet_escape(src, sizeof(src), dst, 2, &used), 1u);
  EXPECT_EQ(used, 1u);
  EXPECT_EQ(telnet_escape(src, sizeof(src), dst, sizeof(dst), &used), 4u);
  EXPECT_EQ(used, 3u);
  const uint8_t want[] = {0x01, TELNET_IAC, TELNET_IAC, 0x02};
  for (size_t i = 0; i < sizeof(want); i++) {
    EXPECT_EQ(dst[i], want[i]);
  }
}

TEST(Rfc2217Telnet, CommandValueIsEscaped) {
  uint8_t dst[COM_PORT_COMMAND_MAX];
  const uint8_t value[] = {0x00, 0x00, 0xFF, 0x00};
  size_t n = write_com_port(dst, COM_SET_BAUDRATE, value, sizeof(value));
  const uint8_t want[] = {TELNET_IAC, TELNET_SB, OPTION_COM_PORT, COM_SET_BAUDRATE, 0x00, 0x00, 0xFF,
                          0xFF,       0x00,      TELNET_IAC,      TELNET_SE};
  ASSERT_EQ(n, sizeof(want));
  for (size_t i = 0; i < sizeof(want); i++) {
    EXPECT_EQ(dst[i], want[i]);
  }
  const uint8_t all_iac[] = {0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(write_com_port(dst, COM_SET_BAUDRATE, all_iac, sizeof(all_iac)), COM_PORT_COMMAND_MAX);
}

}  // namespace
}  // namespace esphome::rfc2217_uart::testing

#endif  // USE_HOST
