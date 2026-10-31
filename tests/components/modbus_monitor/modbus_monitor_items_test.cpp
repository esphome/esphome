#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus_monitor/modbus_monitor.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus_monitor::testing {

namespace {

/// Receive side of tests/components/modbus/common.h's InjectableUART: the frames a listening hub reads, CRC
/// included. A test folder cannot include another component's, so the few lines are repeated here.
class ListenUART : public uart::UARTComponent {
 public:
  ListenUART() {
    this->set_baud_rate(9600);
    this->set_data_bits(8);
    this->set_stop_bits(1);
    this->set_parity(uart::UART_CONFIG_PARITY_NONE);
  }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override {
    if (this->rx_.empty())
      return false;
    *data = this->rx_.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_.size())
      return false;
    memcpy(data, this->rx_.data(), len);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + len);
    return true;
  }
  size_t available() override { return this->rx_.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  void check_logger_conflict() override {}

  // Queues a complete wire frame: address + PDU + CRC16 (low byte first).
  void inject_frame(uint8_t address, std::span<const uint8_t> pdu) {
    size_t start = this->rx_.size();
    this->rx_.push_back(address);
    this->rx_.insert(this->rx_.end(), pdu.begin(), pdu.end());
    uint16_t crc = crc16(this->rx_.data() + start, this->rx_.size() - start);
    this->rx_.push_back(crc & 0xFF);
    this->rx_.push_back(crc >> 8);
  }

 private:
  std::vector<uint8_t> rx_;
};

/// Stands in for a platform item. The sensor platform decodes the bytes and publishes; what is under test
/// here is which items are fed, with what, and when. Records the first register, signed or not, and the
/// bytes it was handed.
class RecordingItem : public MonitorItem {
 public:
  RecordingItem(uint8_t address, uint16_t reg, bool is_signed, uint8_t register_count = 1)
      : MonitorItem(address, reg, register_count), is_signed_(is_signed) {}

  void parse_and_publish(std::span<const uint8_t> data) override {
    const auto raw = static_cast<uint16_t>((data[0] << 8) | data[1]);
    this->published.push_back(this->is_signed_ ? static_cast<float>(static_cast<int16_t>(raw))
                                               : static_cast<float>(raw));
    this->bytes.emplace_back(data.begin(), data.end());
  }

  std::vector<float> published;
  std::vector<std::vector<uint8_t>> bytes;

 private:
  bool is_signed_;
};

/// A listen-only server hub (no local devices) with the decoder on its on_response, wired as
/// codegen wires it.
class SensorFixture {
 public:
  SensorFixture() {
    hub.set_uart_parent(&uart);
    hub.setup();
    hub.add_on_response_callback(
        [this](uint8_t address, std::span<const uint8_t> request, std::span<const uint8_t> response) {
          this->decoder.on_response(address, request, response);
        });
  }

  /// One frame on the wire, as a peer sent it.
  void receive(uint8_t address, std::span<const uint8_t> pdu) {
    uart.inject_frame(address, pdu);
    hub.loop();
  }

  ListenUART uart;
  modbus::ModbusServerHub hub;
  RegisterDecoder decoder;
};

constexpr uint8_t FC_READ_HOLDING = static_cast<uint8_t>(modbus::FunctionCode::READ_HOLDING_REGISTERS);
constexpr uint8_t FC_READ_INPUT = static_cast<uint8_t>(modbus::FunctionCode::READ_INPUT_REGISTERS);

std::vector<uint8_t> read_request(uint8_t function_code, uint16_t start, uint16_t count) {
  return {function_code, static_cast<uint8_t>(start >> 8), static_cast<uint8_t>(start & 0xFF),
          static_cast<uint8_t>(count >> 8), static_cast<uint8_t>(count & 0xFF)};
}

std::vector<uint8_t> read_response(uint8_t function_code, std::span<const uint16_t> registers) {
  std::vector<uint8_t> pdu{function_code, static_cast<uint8_t>(registers.size() * 2)};
  for (uint16_t value : registers) {
    pdu.push_back(static_cast<uint8_t>(value >> 8));
    pdu.push_back(static_cast<uint8_t>(value & 0xFF));
  }
  return pdu;
}

}  // namespace

// A register inside the block a request covered is published when the reply goes past.
TEST(ModbusMonitorItems, PublishesARegisterFromAMatchingRead) {
  SensorFixture f;
  f.decoder.init_items(1);
  RecordingItem bus_voltage{static_cast<uint8_t>(0x0F), 202, false};
  f.decoder.add_item(&bus_voltage);

  const uint16_t registers[] = {0x0FC8, 0x1004};  // 201, 202
  f.receive(0x0F, read_request(FC_READ_HOLDING, 201, 2));
  f.receive(0x0F, read_response(FC_READ_HOLDING, registers));

  ASSERT_EQ(bus_voltage.published.size(), 1u);
  EXPECT_FLOAT_EQ(bus_voltage.published[0], 0x1004);
}

// A server hub pairs a reply by address and function code only. When a client gives up on read A and sends
// read B before A's reply arrives, that reply is paired with B; its length differs from what B asked for, so
// it is not published under B's registers.
TEST(ModbusMonitorItems, DoesNotPublishALateReplyToAnEarlierRead) {
  SensorFixture f;
  f.decoder.init_items(1);
  RecordingItem s{static_cast<uint8_t>(0x0F), 101, false};
  f.decoder.add_item(&s);

  const uint16_t registers_0_to_9[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  f.receive(0x0F, read_request(FC_READ_HOLDING, 100, 2));             // read B
  f.receive(0x0F, read_response(FC_READ_HOLDING, registers_0_to_9));  // the late reply to A (start 0, 10 registers)

  EXPECT_TRUE(s.published.empty());
}

// S_WORD reinterprets the same 16 bits: a discharging battery reads negative, not ~65000.
TEST(ModbusMonitorItems, SignedValuesAreReinterpretedNotRescaled) {
  SensorFixture f;
  f.decoder.init_items(2);
  RecordingItem signed_power{static_cast<uint8_t>(0x0F), 203, true};
  f.decoder.add_item(&signed_power);
  RecordingItem unsigned_power{static_cast<uint8_t>(0x0F), 203, false};
  f.decoder.add_item(&unsigned_power);

  const uint16_t registers[] = {0xF7B4};  // -2124 as int16_t
  f.receive(0x0F, read_request(FC_READ_HOLDING, 203, 1));
  f.receive(0x0F, read_response(FC_READ_HOLDING, registers));

  ASSERT_EQ(signed_power.published.size(), 1u);
  EXPECT_FLOAT_EQ(signed_power.published[0], -2124.0f);
  ASSERT_EQ(unsigned_power.published.size(), 1u);
  EXPECT_FLOAT_EQ(unsigned_power.published[0], 63412.0f);
}

// Two devices on one bus. A sensor only answers to its own address, or a meter register would be
// published from a battery reply that happens to cover the same number.
TEST(ModbusMonitorItems, AddressSelectsBetweenDevicesOnTheSameBus) {
  SensorFixture f;
  f.decoder.init_items(2);
  RecordingItem battery{static_cast<uint8_t>(0x0F), 40018, false};
  f.decoder.add_item(&battery);
  RecordingItem meter{static_cast<uint8_t>(0x02), 40018, true};
  f.decoder.add_item(&meter);

  const uint16_t registers[] = {0xFFB0};  // -80
  f.receive(0x02, read_request(FC_READ_INPUT, 40018, 1));
  f.receive(0x02, read_response(FC_READ_INPUT, registers));

  ASSERT_EQ(meter.published.size(), 1u);
  EXPECT_FLOAT_EQ(meter.published[0], -80.0f);
  EXPECT_TRUE(battery.published.empty());
}

// A register outside the block this exchange covered is not published from it.
TEST(ModbusMonitorItems, IgnoresRegistersTheExchangeDidNotCover) {
  SensorFixture f;
  f.decoder.init_items(3);
  RecordingItem before_block{static_cast<uint8_t>(0x0F), 200, false};
  f.decoder.add_item(&before_block);
  RecordingItem after_block{static_cast<uint8_t>(0x0F), 210, false};
  f.decoder.add_item(&after_block);
  RecordingItem inside{static_cast<uint8_t>(0x0F), 202, false};
  f.decoder.add_item(&inside);

  const uint16_t registers[] = {1, 2, 3};  // 201..203
  f.receive(0x0F, read_request(FC_READ_HOLDING, 201, 3));
  f.receive(0x0F, read_response(FC_READ_HOLDING, registers));

  EXPECT_TRUE(before_block.published.empty());
  EXPECT_TRUE(after_block.published.empty());
  EXPECT_EQ(inside.published.size(), 1u);
}

// Input registers (0x04) decode exactly as holding registers (0x03) do.
TEST(ModbusMonitorItems, PublishesFromInputRegisterReads) {
  SensorFixture f;
  f.decoder.init_items(1);
  RecordingItem s{static_cast<uint8_t>(0x02), 40003, false};
  f.decoder.add_item(&s);

  const uint16_t registers[] = {0xBEEF};
  f.receive(0x02, read_request(FC_READ_INPUT, 40003, 1));
  f.receive(0x02, read_response(FC_READ_INPUT, registers));

  ASSERT_EQ(s.published.size(), 1u);
  EXPECT_FLOAT_EQ(s.published[0], 0xBEEF);
}

// An exception reply carries an error code where the registers would be; publishing from it would
// report the error as a measurement. Through the bus this cannot happen at all: the hub pairs a reply
// only when its function code matches the request's, and an exception's has the 0x80 bit set. Fed to the
// decoder directly instead. Note this holds even without the decoder's exception check: the payload is the
// one exception-code byte, shorter than any register, so the bounds check already rejects it (verified by
// deleting the check; this test still passes). The check is kept as the stated reason, not the only guard.
TEST(ModbusMonitorItems, DoesNotPublishFromAnExceptionReply) {
  RegisterDecoder decoder;
  decoder.init_items(1);
  RecordingItem s{static_cast<uint8_t>(0x0F), 202, false};
  decoder.add_item(&s);

  const auto request = read_request(FC_READ_HOLDING, 202, 1);
  const uint8_t exception[] = {FC_READ_HOLDING | 0x80, 0x02};  // ILLEGAL_DATA_ADDRESS
  decoder.on_response(0x0F, request, exception);

  EXPECT_TRUE(s.published.empty());
}

constexpr uint8_t FC_WRITE_SINGLE = static_cast<uint8_t>(modbus::FunctionCode::WRITE_SINGLE_REGISTER);
constexpr uint8_t FC_WRITE_MULTIPLE = static_cast<uint8_t>(modbus::FunctionCode::WRITE_MULTIPLE_REGISTERS);

// A single write is published from its request once the device echoes it back.
TEST(ModbusMonitorItems, PublishesAConfirmedSingleWrite) {
  SensorFixture f;
  f.decoder.init_items(1);
  RecordingItem shutdown{static_cast<uint8_t>(0x0F), 1101, false};
  f.decoder.add_item(&shutdown);

  const std::vector<uint8_t> write{FC_WRITE_SINGLE, 0x04, 0x4D, 0x00, 0x01};  // 1101 <- 1
  f.receive(0x0F, write);
  f.receive(0x0F, write);  // the echo

  ASSERT_EQ(shutdown.published.size(), 1u);
  EXPECT_FLOAT_EQ(shutdown.published[0], 1.0f);
}

// A multiple write is published from its request once the device repeats the start address and count, and
// feeds every register it covered.
TEST(ModbusMonitorItems, PublishesAConfirmedMultipleWrite) {
  SensorFixture f;
  f.decoder.init_items(3);
  RecordingItem first{static_cast<uint8_t>(0x0F), 1108, false};
  f.decoder.add_item(&first);
  RecordingItem second{static_cast<uint8_t>(0x0F), 1109, false};
  f.decoder.add_item(&second);
  RecordingItem past_end{static_cast<uint8_t>(0x0F), 1110, false};
  f.decoder.add_item(&past_end);

  // 1108 <- 7000, 1109 <- 5000
  f.receive(0x0F, std::vector<uint8_t>{FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x02, 0x04, 0x1B, 0x58, 0x13, 0x88});
  f.receive(0x0F, std::vector<uint8_t>{FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x02});

  ASSERT_EQ(first.published.size(), 1u);
  EXPECT_FLOAT_EQ(first.published[0], 7000.0f);
  ASSERT_EQ(second.published.size(), 1u);
  EXPECT_FLOAT_EQ(second.published[0], 5000.0f);
  EXPECT_TRUE(past_end.published.empty());
}

// The hub takes the next write for a missing reply. That frame is not an echo of the first, so the
// unconfirmed write is not published as if it had been accepted.
TEST(ModbusMonitorItems, DoesNotPublishAnUnconfirmedWrite) {
  SensorFixture f;
  f.decoder.init_items(1);
  RecordingItem s{static_cast<uint8_t>(0x0F), 1101, false};
  f.decoder.add_item(&s);

  f.receive(0x0F, std::vector<uint8_t>{FC_WRITE_SINGLE, 0x04, 0x4D, 0x00, 0x01});
  f.receive(0x0F, std::vector<uint8_t>{FC_WRITE_SINGLE, 0x04, 0x4D, 0x00, 0x02});  // taken for the reply

  EXPECT_TRUE(s.published.empty());
}

// A multiple write whose reply does not repeat its start address and count was not confirmed.
TEST(ModbusMonitorItems, DoesNotPublishAnUnconfirmedMultipleWrite) {
  RegisterDecoder decoder;
  decoder.init_items(1);
  RecordingItem s{static_cast<uint8_t>(0x0F), 1108, false};
  decoder.add_item(&s);

  const uint8_t request[] = {FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x02, 0x04, 0x1B, 0x58, 0x13, 0x88};
  const uint8_t other_count[] = {FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x01};
  decoder.on_response(0x0F, request, other_count);

  EXPECT_TRUE(s.published.empty());
}

// A multiple write whose byte count claims more than it carries publishes only the registers present.
TEST(ModbusMonitorItems, MultipleWriteIsBoundedByItsData) {
  RegisterDecoder decoder;
  decoder.init_items(2);
  RecordingItem first{static_cast<uint8_t>(0x0F), 1108, false};
  decoder.add_item(&first);
  RecordingItem second{static_cast<uint8_t>(0x0F), 1109, false};
  decoder.add_item(&second);

  const uint8_t request[] = {FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x02, 0x04, 0x1B, 0x58};  // second value missing
  const uint8_t reply[] = {FC_WRITE_MULTIPLE, 0x04, 0x54, 0x00, 0x02};
  decoder.on_response(0x0F, request, reply);

  ASSERT_EQ(first.published.size(), 1u);
  EXPECT_FLOAT_EQ(first.published[0], 7000.0f);
  EXPECT_TRUE(second.published.empty());
}

// A value wider than one register is handed all of its registers, and only from an exchange that reached
// the last of them.
TEST(ModbusMonitorItems, WideItemNeedsAllItsRegisters) {
  SensorFixture f;
  f.decoder.init_items(2);
  RecordingItem inside{static_cast<uint8_t>(0x02), 40001, false, 2};
  f.decoder.add_item(&inside);
  RecordingItem straddles_end{static_cast<uint8_t>(0x02), 40002, false, 2};
  f.decoder.add_item(&straddles_end);

  const uint16_t registers[] = {0x1234, 0x5678};  // 40001, 40002
  f.receive(0x02, read_request(FC_READ_HOLDING, 40001, 2));
  f.receive(0x02, read_response(FC_READ_HOLDING, registers));

  ASSERT_EQ(inside.bytes.size(), 1u);
  EXPECT_EQ(inside.bytes[0], (std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78}));
  EXPECT_TRUE(straddles_end.bytes.empty());
}

// One exchange feeds every sensor it covers, so a block read publishes them all in one pass.
TEST(ModbusMonitorItems, OneExchangeFeedsEverySensorItCovers) {
  SensorFixture f;
  f.decoder.init_items(3);
  RecordingItem a{static_cast<uint8_t>(0x0F), 201, false};
  f.decoder.add_item(&a);
  RecordingItem b{static_cast<uint8_t>(0x0F), 203, true};
  f.decoder.add_item(&b);
  RecordingItem c{static_cast<uint8_t>(0x0F), 206, false};
  f.decoder.add_item(&c);

  const uint16_t registers[] = {11, 12, 0xFFFF, 14, 15, 16};  // 201..206
  f.receive(0x0F, read_request(FC_READ_HOLDING, 201, 6));
  f.receive(0x0F, read_response(FC_READ_HOLDING, registers));

  ASSERT_EQ(a.published.size(), 1u);
  EXPECT_FLOAT_EQ(a.published[0], 11.0f);
  ASSERT_EQ(b.published.size(), 1u);
  EXPECT_FLOAT_EQ(b.published[0], -1.0f);  // signed
  ASSERT_EQ(c.published.size(), 1u);
  EXPECT_FLOAT_EQ(c.published[0], 16.0f);
}

}  // namespace esphome::modbus_monitor::testing
