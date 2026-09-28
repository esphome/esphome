#include "common.h"

namespace esphome::uart::testing {

TEST(UARTComponentTest, SetGetBaudRate) {
  MockUARTComponent mock;
  mock.set_baud_rate(38400);
  EXPECT_EQ(mock.get_baud_rate(), 38400);
}

TEST(UARTComponentTest, SetGetStopBits) {
  MockUARTComponent mock;
  mock.set_stop_bits(2);
  EXPECT_EQ(mock.get_stop_bits(), 2);
}

TEST(UARTComponentTest, SetGetDataBits) {
  MockUARTComponent mock;
  mock.set_data_bits(7);
  EXPECT_EQ(mock.get_data_bits(), 7);
}

TEST(UARTComponentTest, SetGetParity) {
  MockUARTComponent mock;
  mock.set_parity(UARTParityOptions::UART_CONFIG_PARITY_EVEN);
  EXPECT_EQ(mock.get_parity(), UARTParityOptions::UART_CONFIG_PARITY_EVEN);
}

TEST(UARTComponentTest, SetGetRxBufferSize) {
  MockUARTComponent mock;
  mock.set_rx_buffer_size(128);
  EXPECT_EQ(mock.get_rx_buffer_size(), 128);
}

TEST(UARTComponentTest, WriteArrayVector) {
  MockUARTComponent mock;
  std::vector<uint8_t> data = {10, 20, 30};
  mock.write_array(data);
  EXPECT_EQ(mock.written_data, data);
}
TEST(UARTComponentTest, WriteByte) {
  MockUARTComponent mock;
  uint8_t byte = 0x79;
  mock.write_byte(byte);
  EXPECT_EQ(mock.written_data.size(), 1);
  EXPECT_EQ(mock.written_data[0], byte);
}

TEST(UARTComponentTest, WriteStr) {
  MockUARTComponent mock;
  const char *str = "Hello";
  std::vector<uint8_t> captured;
  mock.write_str(str);
  EXPECT_EQ(mock.written_data.size(), strlen(str));
  EXPECT_EQ(0, strncmp(str, (const char *) mock.written_data.data(), mock.written_data.size()));
}

// Tests for wrapper methods forwarding to pure virtual read_array
TEST(UARTComponentTest, ReadByteSuccess) {
  MockUARTComponent mock;
  uint8_t value = 0;
  EXPECT_CALL(mock, read_array(&value, 1)).WillOnce(Return(true));
  EXPECT_TRUE(mock.read_byte(&value));
}

TEST(UARTComponentTest, ReadByteFailure) {
  MockUARTComponent mock;
  uint8_t value = 0xFF;
  EXPECT_CALL(mock, read_array(&value, 1)).WillOnce(Return(false));
  EXPECT_FALSE(mock.read_byte(&value));
}

}  // namespace esphome::uart::testing
