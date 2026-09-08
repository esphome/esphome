#include <gtest/gtest.h>

#include <cstring>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus {

using testing::InjectableUART;

namespace {

// Reads holding register 0x0000, one register. The device below answers 0x1234, so the reply on the wire is
// fully determined: address, function code, byte count, value, CRC.
constexpr uint8_t READ_HOLDING_PDU[] = {0x03, 0x00, 0x00, 0x00, 0x01};

// A server device that answers one holding register and can turn its own hub, which is what a device
// announcing a restart needs while the main loop is not running.
class ServicingDevice : public ModbusServerDevice {
 public:
  explicit ServicingDevice(uint8_t address) { this->set_address(address); }

  ResponseStatus on_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                           RegisterValues &registers) override {
    const int reads_on_entry = ++this->reads;
    if (this->service_from_handler)
      this->serviced_from_handler = this->service_bus_();
    if (this->loop_from_handler) {
      this->loop_from_handler = false;  // one level only
      this->hub_->loop();
      this->serviced_after_nested_loop = this->service_bus_();
    }
    // A pass that ran from here would have answered the next frame before this one returned.
    this->reentered |= this->reads != reads_on_entry;
    for (uint16_t i = 0; i < number_of_registers; i++)
      registers.push_back(0x1234);
    return std::nullopt;
  }

  bool pump() { return this->service_bus_(); }

  int reads{0};
  bool service_from_handler{false};
  bool serviced_from_handler{true};
  bool reentered{false};
  // Turns the hub from inside the handler through the public loop(), then asks whether the guard still holds.
  bool loop_from_handler{false};
  bool serviced_after_nested_loop{true};
};

// Lets a test decide when the wire is free, and stash a reply the way send_raw_ does when it is not. Stashed
// directly because send_raw_'s own deferral arms a scheduler timeout, and reaching the scheduler from the
// host test binary faults on an uninitialised lock.
class TestServerHub : public ModbusServerHub {
 public:
  bool tx_blocked() override { return this->blocked; }

  void stash_deferred_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->deferred_payload_[0] = address;
    std::memcpy(this->deferred_payload_.data() + 1, pdu.data(), pdu.size());
    this->deferred_payload_len_ = static_cast<uint16_t>(pdu.size() + 1);
  }

  // Without this every reply waits out the real interframe delay.
  void prime_send_timestamps_for_test() {
    const uint32_t now = millis();
    this->last_modbus_byte_ = now;
    this->last_send_ = now;
  }

  bool blocked{false};
};

struct ServerFixture {
  ServerFixture() {
    hub.set_uart_parent(&uart);
    hub.setup();
    hub.prime_send_timestamps_for_test();
  }

  InjectableUART uart;
  TestServerHub hub;
};

}  // namespace

// Registration is what hands a device its hub, and a device that has one can turn it: the injected frame is
// answered from service_bus_() alone, with no call to the hub's loop() from the test. Every registered device
// gets its own hub, not just the first one.
TEST(ModbusServerService, ServiceBusRunsAPassForEveryRegisteredDevice) {
  ServerFixture f;
  ServicingDevice device(0x02);
  ServicingDevice second(0x03);
  f.hub.register_device(&device);
  f.hub.register_device(&second);

  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  EXPECT_TRUE(device.pump());
  EXPECT_EQ(device.reads, 1);
  // Address, function code, byte count, the register value, then the CRC.
  ASSERT_EQ(f.uart.written.size(), 7u);
  EXPECT_EQ(f.uart.written[0], 0x02);
  EXPECT_EQ(f.uart.written[1], 0x03);
  EXPECT_EQ(f.uart.written[2], 0x02);
  EXPECT_EQ(f.uart.written[3], 0x12);
  EXPECT_EQ(f.uart.written[4], 0x34);
  EXPECT_TRUE(second.pump());
}

// A device that was never registered has no hub to turn, and says so instead of reaching through a null
// pointer.
TEST(ModbusServerService, ServiceBusIsRefusedWithoutAHub) {
  ServicingDevice device(0x02);
  EXPECT_FALSE(device.pump());
}

// Turning the hub from inside a handler would answer a later frame before the one being handled. The second
// frame below is what a re-entrant pass would reach for, and it has to wait its turn. The refusal lasts only
// as long as the dispatch: a hub left marked as dispatching would be deaf to every later call.
TEST(ModbusServerService, ServiceBusIsRefusedFromInsideAHandlerOnly) {
  ServerFixture f;
  ServicingDevice device(0x02);
  device.service_from_handler = true;
  f.hub.register_device(&device);

  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.hub.loop();
  EXPECT_FALSE(device.serviced_from_handler);
  // Both frames are answered by this one pass, but one after the other rather than one inside the other.
  EXPECT_EQ(device.reads, 2);
  EXPECT_FALSE(device.reentered);

  device.service_from_handler = false;
  EXPECT_TRUE(device.pump());
}

// What held a reply back is usually bytes still arriving, and a pass does not drain them. The reply is kept
// for the next one rather than spending its only attempt on a wire that cannot take it, and goes out there:
// the scheduler that would otherwise send it does not run during the waits service_bus_() exists for.
TEST(ModbusServerService, HeldBackReplyIsKeptWhileTheWireIsBusy) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);

  f.hub.blocked = true;
  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());

  f.hub.blocked = false;
  EXPECT_TRUE(device.pump());
  EXPECT_FALSE(f.uart.written.empty());
}

// The held-back reply belongs to one request. Sending it twice would put a stale answer on the wire behind a
// newer one.
TEST(ModbusServerService, HeldBackReplyIsSentOnlyOnce) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);

  device.pump();
  const size_t after_first = f.uart.written.size();
  ASSERT_GT(after_first, 0u);

  device.pump();
  EXPECT_EQ(f.uart.written.size(), after_first);
}

// With nothing held back and nothing arriving, a pass says nothing. Reading a reply out of an empty buffer
// would build a frame from a length of zero.
TEST(ModbusServerService, PassWithNothingPendingWritesNothing) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);

  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());
  EXPECT_EQ(device.reads, 0);
}

// A reply held back earlier goes out before the pass can produce a newer one, so the two cannot reach the
// wire in the wrong order.
TEST(ModbusServerService, HeldBackReplyGoesOutBeforeTheNewOne) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x02, READ_HOLDING_PDU);

  EXPECT_TRUE(device.pump());
  ASSERT_GE(f.uart.written.size(), 8u);
  // The stashed frame is the request echoed back, so its third byte is 0x00; the pass's own reply carries a
  // byte count of 0x02 there.
  EXPECT_EQ(f.uart.written[2], 0x00);
}

// The guard is restored rather than cleared, so a nested pass through the public loop() cannot release the
// dispatch the outer frame is still inside.
TEST(ModbusServerService, ANestedPassDoesNotReleaseTheOuterDispatch) {
  ServerFixture f;
  ServicingDevice device(0x02);
  device.loop_from_handler = true;
  f.hub.register_device(&device);

  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.hub.loop();
  EXPECT_FALSE(device.serviced_after_nested_loop);
}

}  // namespace esphome::modbus
