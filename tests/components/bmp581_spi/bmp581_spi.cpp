#include "common.h"

namespace esphome::bmp581_spi::testing {

// ---------------------------------------------------------------------------
// bmp_read_byte tests
// ---------------------------------------------------------------------------

// The BMP581 SPI protocol requires setting the MSB of the register address to
// indicate a read operation. Verify that bmp_read_byte ORs the register address
// with SPI_READ_BIT before transmitting.
TEST(BMP581SPIReadByteTest, ReadByteEncodesReadBitInRegister) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  uint8_t data = SPI_DUMMY_BYTE;
  comp.bmp_read_byte(bmp581_base::BMP581_CHIP_ID, &data);

  ASSERT_GE(mock.sent_bytes.size(), 1u);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_CHIP_ID) | SPI_READ_BIT);
}

// After the address byte, bmp_read_byte must clock out one byte by sending a
// zero dummy byte. The device drives the MISO line during this transfer.
TEST(BMP581SPIReadByteTest, ReadByteSendsDummyByteToClockResponse) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  uint8_t data = SPI_DUMMY_BYTE;
  comp.bmp_read_byte(bmp581_base::BMP581_INT_STATUS, &data);

  ASSERT_EQ(mock.sent_bytes.size(), 2u);
  EXPECT_EQ(mock.sent_bytes[1], SPI_DUMMY_BYTE);
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

// Each read is a single SPI transaction: CS must be asserted before the first
// byte and de-asserted after the last byte.
TEST(BMP581SPIReadByteTest, ReadByteIsOneSPITransaction) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  uint8_t data = SPI_DUMMY_BYTE;
  bool result = comp.bmp_read_byte(bmp581_base::BMP581_STATUS, &data);

  EXPECT_TRUE(result);
  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
}

// ---------------------------------------------------------------------------
// bmp_write_byte tests
// ---------------------------------------------------------------------------

// For write operations the MSB of the register address must be cleared
// (AND with SPI_WRITE_MASK). All BMP581 register addresses already have
// bit 7 clear, so the encoded byte equals the original address.
TEST(BMP581SPIWriteByteTest, WriteByteEncodesWriteMaskInRegister) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  comp.bmp_write_byte(bmp581_base::BMP581_OSR, static_cast<uint8_t>(bmp581_base::OVERSAMPLING_X4));

  ASSERT_GE(mock.sent_bytes.size(), 1u);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_OSR) & SPI_WRITE_MASK);
}

// The data byte must follow the register address byte on the SPI bus.
TEST(BMP581SPIWriteByteTest, WriteByteTransmitsDataAfterRegister) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr uint8_t write_value = static_cast<uint8_t>(bmp581_base::IIR_FILTER_4);
  comp.bmp_write_byte(bmp581_base::BMP581_DSP_IIR, write_value);

  ASSERT_EQ(mock.sent_bytes.size(), 2u);
  EXPECT_EQ(mock.sent_bytes[1], write_value);
}

// Each write is a single SPI transaction.
TEST(BMP581SPIWriteByteTest, WriteByteIsOneSPITransaction) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  bool result = comp.bmp_write_byte(bmp581_base::BMP581_COMMAND, static_cast<uint8_t>(bmp581_base::RESET_COMMAND));

  EXPECT_TRUE(result);
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

// The entire multi-byte read must be wrapped in a single CS assertion.
TEST(BMP581SPIReadBytesTest, ReadBytesIsOneSPITransaction) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr size_t read_len = 6;
  uint8_t buf[read_len] = {};
  comp.bmp_read_bytes(bmp581_base::BMP581_MEASUREMENT_DATA, buf, read_len);

  EXPECT_EQ(mock.begin_count, 1);
  EXPECT_EQ(mock.end_count, 1);
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
  comp.bmp_write_bytes(bmp581_base::BMP581_DSP, data, write_len);

  ASSERT_EQ(mock.sent_bytes.size(), write_len + 1);
  EXPECT_EQ(mock.sent_bytes[0], static_cast<uint8_t>(bmp581_base::BMP581_DSP) & SPI_WRITE_MASK);
  EXPECT_EQ(mock.sent_bytes[1], data[0]);
  EXPECT_EQ(mock.sent_bytes[2], data[1]);
}

// The entire multi-byte write must be one SPI transaction.
TEST(BMP581SPIWriteBytesTest, WriteBytesIsOneSPITransaction) {
  MockSPIDelegate mock;
  TestBMP581SPIComponent comp;
  comp.set_test_delegate(&mock);

  constexpr size_t write_len = 2;
  uint8_t data[write_len] = {static_cast<uint8_t>(bmp581_base::IIR_FILTER_2),
                             static_cast<uint8_t>(bmp581_base::IIR_FILTER_64)};
  bool result = comp.bmp_write_bytes(bmp581_base::BMP581_DSP, data, write_len);

  EXPECT_TRUE(result);
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
