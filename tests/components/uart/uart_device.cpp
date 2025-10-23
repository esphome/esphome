#include "common.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart::testing {

TEST(UARTDeviceTest, ReadByteSuccess) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  uint8_t value = 0;
  EXPECT_CALL(mock, read_array(_, 1)).WillOnce(DoAll(SetArgPointee<0>(0x5A), Return(true)));
  bool result = dev.read_byte(&value);
  EXPECT_TRUE(result);
  EXPECT_EQ(value, 0x5A);
}

TEST(UARTDeviceTest, ReadByteFailure) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  uint8_t value = 0xFF;
  EXPECT_CALL(mock, read_array(_, 1)).WillOnce(Return(false));
  bool result = dev.read_byte(&value);
  EXPECT_FALSE(result);
}

TEST(UARTDeviceTest, PeekByteSuccess) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  uint8_t value = 0;
  EXPECT_CALL(mock, peek_byte(_)).WillOnce(DoAll(SetArgPointee<0>(0xA5), Return(true)));
  bool result = dev.peek_byte(&value);
  EXPECT_TRUE(result);
  EXPECT_EQ(value, 0xA5);
}

TEST(UARTDeviceTest, PeekByteFailure) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  uint8_t value = 0;
  EXPECT_CALL(mock, peek_byte(_)).WillOnce(Return(false));
  bool result = dev.peek_byte(&value);
  EXPECT_FALSE(result);
}

TEST(UARTDeviceTest, Available) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  EXPECT_CALL(mock, available()).WillOnce(Return(5));
  EXPECT_EQ(dev.available(), 5);
}

TEST(UARTDeviceTest, FlushCallsParent) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  EXPECT_CALL(mock, flush()).Times(1);
  dev.flush();
}

TEST(UARTDeviceTest, WriteByteForwardsToWriteArray) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  dev.write_byte(0xAB);
  EXPECT_EQ(mock.written_data.size(), 1);
  EXPECT_EQ(mock.written_data[0], 0xAB);
}
TEST(UARTDeviceTest, WriteArrayPointer) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  uint8_t data[3] = {1, 2, 3};
  dev.write_array(data, 3);
  EXPECT_EQ(mock.written_data.size(), 3);
  EXPECT_EQ(mock.written_data, std::vector(data, data + 3));
}

TEST(UARTDeviceTest, WriteArrayVector) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  std::vector<uint8_t> data = {4, 5, 6};
  dev.write_array(data);
  EXPECT_EQ(mock.written_data, data);
}

TEST(UARTDeviceTest, WriteArrayStdArray) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  std::array<uint8_t, 4> data = {7, 8, 9, 10};
  dev.write_array(data);
  EXPECT_EQ(mock.written_data.size(), data.size());
  EXPECT_EQ(mock.written_data, std::vector(data.begin(), data.end()));
}

TEST(UARTDeviceTest, WriteStrForwardsToWriteArray) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  const char *str = "ESPHome";
  dev.write_str(str);
  EXPECT_EQ(mock.written_data.size(), strlen(str));
  EXPECT_EQ(0, strncmp(str, (const char *) mock.written_data.data(), mock.written_data.size()));
}

TEST(UARTDeviceTest, WriteStrEmptyString) {
  MockUARTComponent mock;
  UARTDevice dev(&mock);
  const char *str = "";
  dev.write_str(str);
  EXPECT_EQ(mock.written_data.size(), 0);
}

}  // namespace esphome::uart::testing
