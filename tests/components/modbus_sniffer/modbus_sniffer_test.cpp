#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

namespace esphome::modbus::testing {

namespace {

/// One observed exchange, captured from the on_response trigger. The PDUs are spans over the hub's
/// own buffers, so they are copied out here rather than retained.
struct Captured {
  uint8_t address;
  std::vector<uint8_t> request_pdu;
  std::vector<uint8_t> response_pdu;
};

/// Stands in for the lambda a user writes under `on_response:`.
class CaptureResponse : public Action<uint8_t, std::span<const uint8_t>, std::span<const uint8_t>> {
 public:
  explicit CaptureResponse(std::vector<Captured> *out) : out_(out) {}

 protected:
  void play(const uint8_t &address, const std::span<const uint8_t> &request_pdu,
            const std::span<const uint8_t> &response_pdu) override {
    this->out_->push_back(
        Captured{address, {request_pdu.begin(), request_pdu.end()}, {response_pdu.begin(), response_pdu.end()}});
  }

 private:
  std::vector<Captured> *out_;
};

/// Stands in for `on_request:`, which fires whether or not a reply follows.
class CaptureRequest : public Action<uint8_t, std::span<const uint8_t>> {
 public:
  explicit CaptureRequest(std::vector<Captured> *out) : out_(out) {}

 protected:
  void play(const uint8_t &address, const std::span<const uint8_t> &request_pdu) override {
    this->out_->push_back(Captured{address, {request_pdu.begin(), request_pdu.end()}, {}});
  }

 private:
  std::vector<Captured> *out_;
};

/// Hub plus the trigger plumbing the generated automations would set up.
class SnifferFixture {
 public:
  SnifferFixture() : requests_(hub.get_request_trigger()), responses_(hub.get_response_trigger()) {
    hub.set_uart_parent(&uart);
    requests_.add_action(new CaptureRequest(&seen_requests));  // NOLINT(cppcoreguidelines-owning-memory)
    responses_.add_action(new CaptureResponse(&captured));     // NOLINT(cppcoreguidelines-owning-memory)
  }

  /// One loop() reads and parses one batch, so one pass per injected frame, plus one to settle.
  void run(size_t passes = 4) {
    for (size_t i = 0; i < passes; i++)
      hub.loop();
  }

  ModbusSnifferHub hub;
  SnifferUART uart;
  std::vector<Captured> captured;
  std::vector<Captured> seen_requests;

 private:
  Automation<uint8_t, std::span<const uint8_t>> requests_;
  Automation<uint8_t, std::span<const uint8_t>, std::span<const uint8_t>> responses_;
};

constexpr uint8_t FC_READ_HOLDING = static_cast<uint8_t>(FunctionCode::READ_HOLDING_REGISTERS);
constexpr uint8_t FC_READ_INPUT = static_cast<uint8_t>(FunctionCode::READ_INPUT_REGISTERS);
constexpr uint8_t FC_WRITE_SINGLE = static_cast<uint8_t>(FunctionCode::WRITE_SINGLE_REGISTER);

/// Request PDU for a register read: function code, start address, register count.
std::vector<uint8_t> read_request(uint8_t function_code, uint16_t start, uint16_t count) {
  return {function_code, static_cast<uint8_t>(start >> 8), static_cast<uint8_t>(start & 0xFF),
          static_cast<uint8_t>(count >> 8), static_cast<uint8_t>(count & 0xFF)};
}

/// Request PDU for a single register write: function code, register, value.
std::vector<uint8_t> write_request(uint16_t reg, uint16_t value) {
  return {FC_WRITE_SINGLE, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF),
          static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
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

// The core contract: both halves of the exchange reach the automation, raw. The start address the
// response does not carry is in the request, where a lambda can read it with modbus::helpers.
TEST(ModbusSniffer, DeliversBothHalvesOfTheExchange) {
  SnifferFixture f;
  const uint16_t registers[] = {0x1004, 0xFF30};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 202, 2));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run();

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].address, 0x0F);
  EXPECT_EQ(f.captured[0].request_pdu, std::vector<uint8_t>({FC_READ_HOLDING, 0x00, 0xCA, 0x00, 0x02}));
  EXPECT_EQ(f.captured[0].response_pdu, std::vector<uint8_t>({FC_READ_HOLDING, 0x04, 0x10, 0x04, 0xFF, 0x30}));

  // What a lambda would then do with them.
  EXPECT_EQ(helpers::get_data<uint16_t>(f.captured[0].request_pdu.data(), 1), 202);
  const auto payload = helpers::server_pdu_payload(f.captured[0].response_pdu);
  EXPECT_EQ(payload.size(), 4u);
  EXPECT_EQ(helpers::get_data<uint16_t>(payload.data(), 0), 0x1004);
}

// A sniffer must never disturb the bus it is watching.
TEST(ModbusSniffer, NeverTransmits) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 100, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.uart.inject_frame(0x0F, write_request(1101, 1));
  f.run(6);

  EXPECT_TRUE(f.uart.written.empty());
}

// on_request fires for every request, so a lambda can see polls that are never answered.
TEST(ModbusSniffer, RequestTriggerFiresEvenWithoutAReply) {
  SnifferFixture f;

  f.uart.inject_frame(0x0E, read_request(FC_READ_HOLDING, 300, 1));  // absent device, never answers
  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 400, 1));
  f.run(6);

  ASSERT_EQ(f.seen_requests.size(), 2u);
  EXPECT_EQ(f.seen_requests[0].address, 0x0E);
  EXPECT_EQ(f.seen_requests[1].address, 0x0F);
  EXPECT_TRUE(f.captured.empty());  // neither was answered
}

// Nothing is decoded, so a function code the hub has never heard of pairs like any other. This is
// what storing the PDU whole buys over destructuring it.
TEST(ModbusSniffer, PairsAnyFunctionCodeIncludingWritesAndCoils) {
  struct Case {
    const char *name;
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;
  };
  const std::vector<Case> cases{
      {"write single register", {0x06, 0x04, 0x4D, 0x00, 0x01}, {0x06, 0x04, 0x4D, 0x00, 0x01}},
      {"write multiple registers", {0x10, 0x04, 0x4D, 0x00, 0x01, 0x02, 0x00, 0x01}, {0x10, 0x04, 0x4D, 0x00, 0x01}},
      {"read coils", {0x01, 0x00, 0x13, 0x00, 0x09}, {0x01, 0x02, 0xCD, 0x01}},
      {"read discrete inputs", {0x02, 0x00, 0xC4, 0x00, 0x16}, {0x02, 0x03, 0xAC, 0xDB, 0x35}},
      {"read/write multiple",
       {0x17, 0x00, 0xC8, 0x00, 0x01, 0x01, 0x2C, 0x00, 0x01, 0x02, 0x00, 0x07},
       {0x17, 0x02, 0x12, 0x34}},
      {"exception", {0x03, 0x00, 0xCA, 0x00, 0x02}, {0x83, 0x02}},
  };

  for (const Case &c : cases) {
    SnifferFixture f;
    f.uart.inject_frame(0x0F, c.request);
    f.uart.inject_frame(0x0F, c.response);
    f.run();

    ASSERT_EQ(f.captured.size(), 1u) << c.name;
    EXPECT_EQ(f.captured[0].request_pdu, c.request) << c.name;
    EXPECT_EQ(f.captured[0].response_pdu, c.response) << c.name;
  }
}

// A reply with no retained request cannot be placed, so it is dropped. Happens on the first
// exchange after a reset, or when a request is lost to a CRC error.
TEST(ModbusSniffer, DropsResponseWithNoRetainedRequest) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0042};

  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
}

// One request yields exactly one response; the retained request is consumed by the first reply.
TEST(ModbusSniffer, RetainedRequestIsConsumedByItsFirstResponse) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0007};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 500, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run(8);

  EXPECT_EQ(f.captured.size(), 1u);
}

// One slot is enough because RTU is half duplex: a second request replaces the first, so an
// unanswered poll cannot capture the next device's reply.
TEST(ModbusSniffer, UnansweredPollDoesNotCaptureTheNextDevicesReply) {
  SnifferFixture f;
  const uint16_t registers[] = {0x2222};

  f.uart.inject_frame(0x0E, read_request(FC_READ_HOLDING, 300, 1));  // absent device, never answers
  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 400, 1));
  f.uart.inject_frame(0x0F, read_response(FC_READ_HOLDING, registers));
  f.run(8);

  ASSERT_EQ(f.captured.size(), 1u);
  EXPECT_EQ(f.captured[0].address, 0x0F);
  EXPECT_EQ(helpers::get_data<uint16_t>(f.captured[0].request_pdu.data(), 1), 400);  // not 300
}

// A reply from an address other than the retained request's is not that request's reply.
TEST(ModbusSniffer, DropsResponseFromADifferentAddress) {
  SnifferFixture f;
  const uint16_t registers[] = {0x0001};

  f.uart.inject_frame(0x0F, read_request(FC_READ_HOLDING, 700, 1));
  f.uart.inject_frame(0x02, read_response(FC_READ_HOLDING, registers));
  f.run();

  EXPECT_TRUE(f.captured.empty());
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

}  // namespace esphome::modbus::testing
