#include "../common.h"

namespace esphome::bmp581_spi::testing {

// ---------------------------------------------------------------------------
// bmp_read_byte tests
// ---------------------------------------------------------------------------

// The BMP581 SPI protocol requires setting the MSB of the register address to
// indicate a read operation. Verify that bmp_read_byte ORs the register address
// with SPI_READ_BIT, then clocks out one dummy byte to capture the response.
TEST(BMP581SPIReadByteTest, ReadByteEncodesReadBitAndClocksDummyByte) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  uint8_t data = SPI_DUMMY_BYTE;
  comp.bmp_read_byte(bmp581_base::BMP581_CHIP_ID, &data);

  ASSERT_EQ(mock.sent_bytes.size(), 2u);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_CHIP_ID) | SPI_READ_BIT);
  EXPECT_EQ(mock.sent_bytes[1], SPI_DUMMY_BYTE);
  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

// The byte returned by the device (mocked as the transfer response) must be
// stored in the output pointer.
TEST(BMP581SPIReadByteTest, ReadByteCapturesResponseInOutputPointer) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  // First transfer (address byte) returns 0; second (dummy) returns the sensor value.
  mock.push_response(SPI_DUMMY_BYTE);
  mock.push_response(bmp581_base::BMP581_ASIC_ID);

  uint8_t data = SPI_DUMMY_BYTE;
  comp.bmp_read_byte(bmp581_base::BMP581_CHIP_ID, &data);

  EXPECT_EQ(data, bmp581_base::BMP581_ASIC_ID);
}

// ---------------------------------------------------------------------------
// bmp_write_byte tests
// ---------------------------------------------------------------------------

// For write operations the MSB of the register address must be cleared
// (AND with SPI_WRITE_MASK), followed by the data byte.
TEST(BMP581SPIWriteByteTest, WriteByteEncodesWriteMaskAndTransmitsData) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr uint8_t write_value = static_cast<uint8_t>(bmp581_base::IIR_FILTER_4);
  comp.bmp_write_byte(bmp581_base::BMP581_DSP_IIR, write_value);

  ASSERT_EQ(mock.sent_bytes.size(), 2u);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_DSP_IIR) & SPI_WRITE_MASK);
  EXPECT_EQ(mock.sent_bytes[1], write_value);
  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

// ---------------------------------------------------------------------------
// bmp_read_bytes tests
// ---------------------------------------------------------------------------

// bmp_read_bytes must send the register address with the read bit set, then
// clock out len bytes by sending len dummy zeros.
TEST(BMP581SPIReadBytesTest, ReadBytesEncodesReadBitAndClocksAllBytes) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr size_t read_len = 3;
  uint8_t buf[read_len] = {};
  comp.bmp_read_bytes(bmp581_base::BMP581_MEASUREMENT_DATA, buf, read_len);

  // First byte: register address with read bit
  ASSERT_EQ(mock.sent_bytes.size(), read_len + 1);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_MEASUREMENT_DATA) | SPI_READ_BIT);

  // Remaining bytes: dummy zeros to clock out the responses
  for (size_t i = 1; i <= read_len; i++) {
    EXPECT_EQ(mock.sent_bytes[i], SPI_DUMMY_BYTE) << "dummy byte index " << i;
  }

  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

// Response bytes returned by the device must be stored into the output buffer
// in order.
TEST(BMP581SPIReadBytesTest, ReadBytesCapturesAllResponsesIntoBuffer) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  // Preset: address transfer returns 0, then three data bytes
  mock.push_response(SPI_DUMMY_BYTE);
  constexpr uint8_t byte0 = static_cast<uint8_t>(bmp581_base::OVERSAMPLING_X8);
  constexpr uint8_t byte1 = static_cast<uint8_t>(bmp581_base::OVERSAMPLING_X16);
  constexpr uint8_t byte2 = static_cast<uint8_t>(bmp581_base::OVERSAMPLING_X32);
  mock.push_response(byte0);
  mock.push_response(byte1);
  mock.push_response(byte2);

  constexpr size_t read_len = 3;
  uint8_t buf[read_len] = {};
  bool result = comp.bmp_read_bytes(bmp581_base::BMP581_MEASUREMENT_DATA, buf, read_len);

  EXPECT_TRUE(result);
  EXPECT_EQ(buf[0], byte0);
  EXPECT_EQ(buf[1], byte1);
  EXPECT_EQ(buf[2], byte2);
}

// ---------------------------------------------------------------------------
// bmp_write_bytes tests
// ---------------------------------------------------------------------------

// bmp_write_bytes must send the register address (write-masked) followed by
// each data byte in the array.
TEST(BMP581SPIWriteBytesTest, WriteBytesEncodesWriteMaskAndTransmitsAllBytes) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr size_t write_len = 2;
  uint8_t data[write_len] = {static_cast<uint8_t>(bmp581_base::IIR_FILTER_8),
                             static_cast<uint8_t>(bmp581_base::IIR_FILTER_16)};
  bool result = comp.bmp_write_bytes(bmp581_base::BMP581_DSP, data, write_len);

  EXPECT_TRUE(result);
  ASSERT_EQ(mock.sent_bytes.size(), write_len + 1);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_DSP) & SPI_WRITE_MASK);
  EXPECT_EQ(mock.sent_bytes[1], data[0]);
  EXPECT_EQ(mock.sent_bytes[2], data[1]);
  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

// ---------------------------------------------------------------------------
// activate_interface tests
// ---------------------------------------------------------------------------

// The BMP581 reverts to I2C mode after a reset. activate_interface() must
// perform a dummy read of the CHIP_ID register to force the device back into
// SPI mode. Verify that the correct register address (with read bit) is sent.
TEST(BMP581SPIActivateInterfaceTest, ActivateInterfaceReadsChipIdRegister) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  comp.call_activate_interface();

  // activate_interface calls bmp_read_byte(BMP581_CHIP_ID, &dummy), which sends
  // the address byte (with read bit) followed by one dummy byte.
  ASSERT_EQ(mock.sent_bytes.size(), 2u);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_CHIP_ID) | SPI_READ_BIT);
  EXPECT_EQ(mock.sent_bytes[1], SPI_DUMMY_BYTE);
  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

}  // namespace esphome::bmp581_spi::testing
