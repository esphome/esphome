#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Records custom-response dispatches so tests can assert an unknown-length frame reached the device.
class CustomRecordingDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          ResponseStatus status) override {
    this->requests.emplace_back(request_pdu.begin(), request_pdu.end());
    this->responses.emplace_back(response_pdu.begin(), response_pdu.end());
    this->statuses.push_back(status);
  }
  std::vector<std::vector<uint8_t>> requests;
  std::vector<std::vector<uint8_t>> responses;
  std::vector<ResponseStatus> statuses;
};

// Every handler keeps its ILLEGAL_FUNCTION default; the hub's dispatch is what is under test.
class SilentServerDevice : public ModbusServerDevice {};

// Drives full client frames through the server hub's receive path (same shape as the broadcast tests).
class TestServerHub : public ModbusServerHub {
 public:
  bool tx_blocked() override { return false; }

  // Builds a complete client frame (address + FC + data + CRC) and runs the full receive-side parser.
  // Returns true once the buffer has fully drained.
  bool run_receive_parser_for_test(uint8_t address, uint8_t function_code, std::span<const uint8_t> data) {
    this->rx_buffer_.clear();
    this->rx_buffer_.reserve(data.size() + 4);
    this->rx_buffer_.push_back(address);
    this->rx_buffer_.push_back(function_code);
    this->rx_buffer_.insert(this->rx_buffer_.end(), data.begin(), data.end());
    uint16_t crc = crc16(this->rx_buffer_.data(), this->rx_buffer_.size());
    this->rx_buffer_.push_back(crc & 0xFF);
    this->rx_buffer_.push_back(crc >> 8);
    this->parse_modbus_frames();
    return this->rx_buffer_.empty();
  }
};

}  // namespace

// The frame-length parsers have explicit cases for exactly these 13 codes; every other value - the
// assigned-but-unimplemented management codes, both user-defined ranges, and all unassigned codes -
// must classify as unknown length. The exception flag masks off first.
TEST(ModbusUnknownFunction, HelperMatchesParserCoverage) {
  for (uint8_t fc : {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0F, 0x10, 0x14, 0x15, 0x16, 0x17, 0x18}) {
    EXPECT_FALSE(helpers::is_function_code_unknown_length(fc)) << "fc 0x" << std::hex << int(fc);
  }
  for (uint8_t fc : {0x07, 0x08, 0x0B, 0x0C, 0x11, 0x2A, 0x41, 0x48, 0x49, 0x64, 0x6E, 0x00, 0x7F}) {
    EXPECT_TRUE(helpers::is_function_code_unknown_length(fc)) << "fc 0x" << std::hex << int(fc);
  }
  // Exception replies classify by their base code.
  EXPECT_FALSE(helpers::is_function_code_unknown_length(0x83));
  EXPECT_TRUE(helpers::is_function_code_unknown_length(0x87));
  // Strictly wider than the user-defined ranges: every custom code is unknown-length, but not vice versa.
  for (int fc = 0; fc <= 0xFF; fc++) {
    if (helpers::is_function_code_custom(fc))
      EXPECT_TRUE(helpers::is_function_code_unknown_length(fc)) << "fc 0x" << std::hex << fc;
  }
  EXPECT_FALSE(helpers::is_function_code_custom(0x49));

  // Derived contract check: the helper must say "unknown" exactly when both length parsers fall
  // through to default. With a zero-filled max-size PDU every explicit case returns at least 2
  // (file records bottom out at 2, FIFO at 3) and only default returns MIN_PDU_SIZE, so comparing
  // against MIN_PDU_SIZE detects a case added to either switch without updating the helper. The
  // loop stops at 0x7F: above it the helper masks the exception flag off while client_pdu_length()
  // switches on the unmasked byte and server_pdu_length() early-returns the exception length.
  for (int fc = 0; fc <= 0x7F; fc++) {
    const uint8_t pdu[MAX_PDU_SIZE] = {static_cast<uint8_t>(fc)};  // zero header fields
    EXPECT_EQ(helpers::is_function_code_unknown_length(fc),
              helpers::client_pdu_length(pdu, sizeof(pdu)) == MIN_PDU_SIZE)
        << "client_pdu_length disagrees for fc 0x" << std::hex << fc;
    EXPECT_EQ(helpers::is_function_code_unknown_length(fc),
              helpers::server_pdu_length(pdu, sizeof(pdu)) == MIN_PDU_SIZE)
        << "server_pdu_length disagrees for fc 0x" << std::hex << fc;
  }
}

// A response with a function code outside the user-defined ranges (0x49) has no length case in
// server_pdu_length(), so the parser must find the frame end by CRC scan - the same way it already
// handles user-defined codes. Frame: address + FC 0x49 + 3 data bytes + CRC = 7 bytes. Without the
// scan the parser assumes a 4-byte frame, fails the CRC, and the response never reaches the device.
TEST(ModbusUnknownFunction, ClientParsesUnknownLengthResponse) {
  InjectableUART uart;
  ModbusClientHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();  // computes frame timing from the baud rate
  CustomRecordingDevice device(&hub, 0x02);

  const uint8_t request[] = {0x49, 0x01};
  ASSERT_TRUE(device.queue_pdu(request));
  hub.loop();  // transmit
  ASSERT_FALSE(uart.written.empty());

  const uint8_t response_pdu[] = {0x49, 0x02, 0xAA, 0xBB};
  uart.inject_frame(0x02, response_pdu);
  hub.loop();  // receive + parse + match + dispatch

  ASSERT_EQ(device.responses.size(), 1u);
  EXPECT_EQ(device.requests[0], std::vector<uint8_t>(request, request + sizeof(request)));
  EXPECT_EQ(device.responses[0], std::vector<uint8_t>(response_pdu, response_pdu + sizeof(response_pdu)));
  EXPECT_FALSE(device.statuses[0].has_value());
}

// The server side of the same gap: a request with FC 0x49 for a registered device must parse (CRC
// scan again) so the hub can answer ILLEGAL_FUNCTION per the spec. Without the scan the frame fails
// to parse and the client gets silence instead of the exception.
TEST(ModbusUnknownFunction, ServerRepliesIllegalFunctionToUnknownLengthRequest) {
  TestServerHub hub;
  RecordingUART uart;
  hub.set_uart_parent(&uart);

  SilentServerDevice device;
  device.set_address(0x02);
  hub.register_device(&device);

  const uint8_t data[] = {0x02, 0xAA, 0xBB};
  ASSERT_TRUE(hub.run_receive_parser_for_test(0x02, 0x49, data));

  // Expected reply: address + FC with exception flag + ILLEGAL_FUNCTION + CRC.
  std::vector<uint8_t> expected = {0x02, 0xC9, 0x01};
  uint16_t crc = crc16(expected.data(), expected.size());
  expected.push_back(crc & 0xFF);
  expected.push_back(crc >> 8);
  EXPECT_EQ(uart.written, expected);
}

}  // namespace esphome::modbus::testing
