#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

namespace esphome::modbus::testing {

namespace {

/// One observed exchange, captured from the on_response trigger.
struct Captured {
  uint8_t address;
  uint8_t function_code;
  uint16_t start_address;
  std::vector<uint8_t> payload;
};

/// Stands in for the lambda a user writes under `on_response:`. The payload is a span over the
/// hub's buffer, so it is copied out here rather than retained.
class CaptureAction : public Action<uint8_t, uint8_t, uint16_t, std::span<const uint8_t>> {
 public:
  explicit CaptureAction(std::vector<Captured> *out) : out_(out) {}

 protected:
  void play(const uint8_t &address, const uint8_t &function_code, const uint16_t &start_address,
            const std::span<const uint8_t> &payload) override {
    this->out_->push_back(
        Captured{address, function_code, start_address, std::vector<uint8_t>(payload.begin(), payload.end())});
  }

 private:
  std::vector<Captured> *out_;
};

/// Hub plus the trigger plumbing a generated `on_response:` would set up.
class SnifferFixture {
 public:
  SnifferFixture() : automation_(hub.get_response_trigger()) {
    hub.set_uart_parent(&uart);
    automation_.add_action(new CaptureAction(&captured));  // NOLINT(cppcoreguidelines-owning-memory)
  }

  /// One loop() reads and parses one batch, so one pass per injected frame, plus one to settle.
  void run(size_t passes = 4) {
    for (size_t i = 0; i < passes; i++)
      hub.loop();
  }

  ModbusSnifferHub hub;
  SnifferUART uart;
  std::vector<Captured> captured;

 private:
  Automation<uint8_t, uint8_t, uint16_t, std::span<const uint8_t>> automation_;
};

constexpr uint8_t FC_READ_HOLDING = static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS);
constexpr uint8_t FC_READ_INPUT = static_cast<uint8_t>(FunctionCode::READ_INPUT_REGISTERS);
constexpr uint8_t FC_WRITE_SINGLE = static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER);
constexpr uint8_t FC_READ_WRITE = static_cast<uint8_t>(FunctionCode::READ_WRITE_MULTIPLE_REGISTERS);

/// Request PDU for a register read: function code, start address, register count.
std::vector<uint8_t> read_request(uint8_t function_code, uint16_t start, uint16_t count) {
  return {function_code, static_cast<uint8_t>(start >> 8), static_cast<uint8_t>(start & 0xFF),
          static_cast<uint8_t>(count >> 8), static_cast<uint8_t>(count & 0xFF)};
}

/// Response PDU for a register read: function code, byte count, then the register data.
std::vector<uint8_t> read_response(uint8_t function_code, std::span<const uint16_t> registers) {
  std::vector<uint8_t> pdu{function_code, static_cast<uint8_t>(registers.size() * 2)};
  for (uint16_t value : registers) {
    pdu.push_back(static_cast<uint8_t>(value >> 8));
    pdu.push_back(static_cast<uint8_t>(value & 0xFF));
  }
  return pdu;
}

}  // namespace

// The core contract: the start address a response does not carry is recovered from its request.
TEST(ModbusSniffer, PairsRequestWithResponseAndRecoversStartAddress) {
  SnifferFixture f;
  const uint16_t registers[] = {0x1004, 0xFF30};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 202, 2));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run();

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].address, 0x0F);
  EXPECT_EQ(f.captured[0].function_code, FC_READ_HOLDING);
  EXPECT_EQ(f.captured[0].start_address, 202);  // from the REQUEST; the response never carries it
  EXPECT_EQ(f.captured[0].payload, std::vector<uint8_t>({0x10, 0x04, 0xFF, 0x30}));
}

// A sniffer must never disturb the bus it is watching.
TEST(ModbusSniffer, NeverTransmits) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 100, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.uart.inject_frame(0x0F, read_request(FC_WRITE_SINGLE, 1101, 1));
  f.run(6);

  EXPECT_TRUE(f.uart.written.empty());
}

// Input registers (0x04) pair exactly as holding registers (0x03) do.
TEST(ModbusSniffer, PairsInputRegisterReads) {
  SnifferFixture f;
  const uint16_t registers[] = {0xBEEF};

  f.uart.inject_frame(0x02, read_request(FC_READ_INPUT, 40018, 1));
  f.uart.inject_frame(0x02, read_response(FC_READ_INPUT, registers));
  f.run();

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].function_code, FC_READ_INPUT);
  EXPECT_EQ(f.captured[0].start_address, 40018);
}

// Why requests are keyed by address: with a single slot, a poll that is never answered would
// still be pending and would capture the next device's reply under the wrong start address.
TEST(ModbusSniffer, UnansweredPollDoesNotCaptureTheNextDevicesReply) {
  SnifferFixture f;
  const uint16_t registers[] = {0x2222};

  f.uart.inject_frame(0x0E, read_request(FC_READ_HOLDING, 300, 1));  // absent device, never answers
  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 400, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run(8);

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].address, 0x0F);
  EXPECT_EQ(f.captured[0].start_address, 400);  // 400, not the absent device's 300
  EXPECT_EQ(f.captured[0].payload, std::vector<uint8_t>({0x22, 0x22}));
}

// The register address is unknowable, so it is counted rather than guessed at. Happens on the
// first exchange after a reset, or when a request is lost to a CRC error.
TEST(ModbusSniffer, DropsResponseWithNoRetainedRequest) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0042};

  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

TEST(ModbusSniffer, RetainedRequestIsConsumedByItsFirstResponse) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0007};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 500, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run(8);

  EXPECT_EQ(f.captured.size(), 1u);
}

// A single-register write is paired: its echo confirms the value the device accepted. The register
// address is dropped from the payload so it starts at start_address, as a read's does.
TEST(ModbusSniffer, PairsSingleRegisterWrite) {
  SnifferFixture f;
  // FC 0x06: register 1101, value 1. The echo a server sends back is byte-identical.
  const std::vector<uint8_t> write_pdu{FC_WRITE_SINGLE, 0x04, 0x4D, 0x00, 0x01};

  f.uart.inject_frame(0x0F, write_pdu);
  f.uart.inject_frame(0x0F, write_pdu);
  f.run();

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].function_code, FC_WRITE_SINGLE);
  EXPECT_EQ(f.captured[0].start_address, 1101);
  EXPECT_EQ(f.captured[0].payload, std::vector<uint8_t>({0x00, 0x01}));  // the value, not the echo
}

// 0x17 reads and writes in one exchange; only the read half comes back, at the same request offsets
// as a plain read.
TEST(ModbusSniffer, PairsReadWriteMultiple) {
  SnifferFixture f;
  // Read 2 registers from 200, write 1 register at 300. Request: read start, read count, write
  // start, write count, write byte count, write data.
  const std::vector<uint8_t> rw_request{FC_READ_WRITE, 0x00, 0xC8, 0x00, 0x02, 0x01,
                                        0x2C,          0x00, 0x01, 0x02, 0x00, 0x07};
  const uint16_t read_back[] = {0x1234, 0x5678};

  f.uart.inject_frame(0x0F, rw_request);
  f.uart.inject_frame(0x0F, read_response(FC_READ_WRITE, read_back));
  f.run();

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].function_code, FC_READ_WRITE);
  EXPECT_EQ(f.captured[0].start_address, 200);  // the READ start, not the write's 300
  EXPECT_EQ(f.captured[0].payload, std::vector<uint8_t>({0x12, 0x34, 0x56, 0x78}));
}

// Multi-register writes are not paired: the echo carries a count, not data.
TEST(ModbusSniffer, DoesNotPairMultiRegisterWrite) {
  SnifferFixture f;
  const uint8_t fc10 = static_cast<uint8_t>(FunctionCode::WRITE_MULTIPLE_REGISTERS);
  // Write 1 register at 1101, value 1.
  const std::vector<uint8_t> req{fc10, 0x04, 0x4D, 0x00, 0x01, 0x02, 0x00, 0x01};
  const std::vector<uint8_t> echo{fc10, 0x04, 0x4D, 0x00, 0x01};

  f.uart.inject_frame(0x0F, req);
  f.uart.inject_frame(0x0F, echo);
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

// What the unconditional arming of expecting_peer_response_ buys: without it the write's echo
// fails to parse as a request and is discarded, taking the next frame's framing with it.
TEST(ModbusSniffer, PairsAReadFollowingAWriteExchange) {
  SnifferFixture f;
  const std::vector<uint8_t> write_pdu{FC_WRITE_SINGLE, 0x04, 0x4D, 0x00, 0x01};
  const uint16_t registers[] = {0x00C8};

  f.uart.inject_frame(0x0F, write_pdu);
  f.uart.inject_frame(0x0F, write_pdu);
  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 206, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run(8);

  ASSERT_EQ(f.captured.size(), 2u);
  EXPECT_EQ(f.captured[1].start_address, 206);
  EXPECT_EQ(f.captured[1].payload, std::vector<uint8_t>({0x00, 0xC8}));
}

// Documented limitation: address 0 is the broadcast address and is never answered.
TEST(ModbusSniffer, DoesNotPairAddressZero) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};

  f.uart.inject_frame(0x00, read_request(FC_READ_HOLDING, 100, 1));
  f.uart.inject_frame(0x00, read_response(FC_READ_HOLDING, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

// Delivered short, the payload would not line up with the start address the consumer indexes from.
TEST(ModbusSniffer, DropsResponseShorterThanRequestedCount) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};  // one register, but four were asked for

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 600, 4));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

TEST(ModbusSniffer, DropsResponseWithMismatchedFunctionCode) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 700, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_INPUT, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

}  // namespace esphome::modbus::testing
