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

// Writes 0x0001 to holding register 0x0000. A broadcast is never answered, so this is a frame the hub parses
// and dispatches without producing a reply of its own.
constexpr uint8_t WRITE_SINGLE_PDU[] = {0x06, 0x00, 0x00, 0x00, 0x01};

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
      // Deliberately the hostile route: a device cannot reach its own hub, so the test holds its own
      // pointer to turn it through the public loop().
      this->hostile_hub->loop();
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
  ModbusServerHub *hostile_hub{nullptr};
};

// Lets a test decide when the wire is free, and stash a reply the way send_raw_ does when it is not. Stashed
// directly because send_raw_'s own deferral arms a scheduler timeout, and reaching the scheduler from the
// host test binary faults on an uninitialised lock.
class TestServerHub : public ModbusServerHub {
 public:
  // block_on_recheck: free at the first check, busy at send_frame_'s own re-check, which is a byte arriving
  // during the send delay. block_first_check: the reverse, so a pass starts on a busy wire and frees up.
  bool tx_blocked() override {
    const int check = this->checks_++;
    if (this->block_on_recheck)
      return check > 0;
    if (this->block_first_check)
      return check == 0;
    return this->blocked;
  }

  void stash_deferred_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->deferred_payload_[0] = address;
    std::memcpy(this->deferred_payload_.data() + 1, pdu.data(), pdu.size());
    this->deferred_payload_len_ = static_cast<uint16_t>(pdu.size() + 1);
  }

  // What the scheduler timeout does, without the scheduler.
  void drop_deferred_for_test() { this->send_deferred_(true); }

  bool has_deferred() const { return this->deferred_payload_len_ != 0; }

  bool blocked{false};
  bool block_on_recheck{false};
  bool block_first_check{false};

 private:
  int checks_{0};
};

struct ServerFixture {
  ServerFixture() {
    hub.set_uart_parent(&uart);
    hub.setup();
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

// The wire can also turn busy between the pass's own check and the send: a byte arriving during the send
// delay. That is still a busy wire, not a reply nobody waits for, so it is kept as well.
TEST(ModbusServerService, ReplyBlockedDuringTheSendDelayIsKept) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);

  f.hub.block_on_recheck = true;
  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());
  EXPECT_TRUE(f.hub.has_deferred());

  f.hub.block_on_recheck = false;
  EXPECT_TRUE(device.pump());
  EXPECT_FALSE(f.uart.written.empty());
}

// A frame the pass answers is newer than anything still held back, so the held one goes when that frame
// arrives. A later pass must not put it on the wire behind the reply that answered the newer request.
TEST(ModbusServerService, AReplySentOnThePassDiscardsAnOlderHeldOne) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x02, READ_HOLDING_PDU);

  // Busy at the start of the pass, so the held reply is kept; free by the time the pass answers the frame.
  f.hub.block_first_check = true;
  EXPECT_TRUE(device.pump());
  f.hub.block_first_check = false;
  ASSERT_EQ(device.reads, 1);
  const size_t after_pass = f.uart.written.size();
  ASSERT_GT(after_pass, 0u);
  EXPECT_FALSE(f.hub.has_deferred());

  // Nothing is left over to be sent behind the reply that just went out.
  EXPECT_TRUE(device.pump());
  EXPECT_EQ(f.uart.written.size(), after_pass);
}

// A broadcast is never answered, so no reply of our own goes out to clear the one still held back. The
// controller has moved on all the same, and the held reply must not reach the wire on a later pass.
TEST(ModbusServerService, ABroadcastDiscardsAHeldReply) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(BROADCAST_ADDRESS, WRITE_SINGLE_PDU);

  // Busy at the start of the pass, so the held reply survives long enough for the broadcast to be parsed.
  f.hub.block_first_check = true;
  EXPECT_TRUE(device.pump());
  f.hub.block_first_check = false;
  // Gone means the broadcast really was parsed: nothing else in this pass touches the stash.
  EXPECT_FALSE(f.hub.has_deferred());

  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());
}

// Same for a request addressed to another device on the wire: we do not answer it, and the reply still held
// back is just as stale as it is after a broadcast.
TEST(ModbusServerService, ARequestToAPeerDiscardsAHeldReply) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x09, READ_HOLDING_PDU);

  f.hub.block_first_check = true;
  EXPECT_TRUE(device.pump());
  f.hub.block_first_check = false;
  EXPECT_FALSE(f.hub.has_deferred());
  EXPECT_EQ(device.reads, 0);

  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());
}

// The scheduler gives the reply its one chance and lets it go. A pass afterwards must not find it and put a
// reply the controller has stopped waiting for on the wire behind newer traffic.
TEST(ModbusServerService, ReplyDroppedByTheSchedulerIsNotSentByALaterPass) {
  ServerFixture f;
  ServicingDevice device(0x02);
  f.hub.register_device(&device);
  f.hub.stash_deferred_for_test(0x02, READ_HOLDING_PDU);

  f.hub.blocked = true;
  f.hub.drop_deferred_for_test();
  EXPECT_TRUE(f.uart.written.empty());
  EXPECT_FALSE(f.hub.has_deferred());

  f.hub.blocked = false;
  EXPECT_TRUE(device.pump());
  EXPECT_TRUE(f.uart.written.empty());
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
  // The stashed frame is the request echoed back (8 bytes, third byte 0x00); the pass's own reply follows it
  // (7 bytes, byte count 0x02 in its third byte).
  ASSERT_EQ(f.uart.written.size(), 15u);
  EXPECT_EQ(f.uart.written[2], 0x00);
  EXPECT_EQ(f.uart.written[8], 0x02);
  EXPECT_EQ(f.uart.written[10], 0x02);
}

// The guard is restored rather than cleared, so a nested pass through the public loop() cannot release the
// dispatch the outer frame is still inside.
TEST(ModbusServerService, ANestedPassDoesNotReleaseTheOuterDispatch) {
  ServerFixture f;
  ServicingDevice device(0x02);
  device.loop_from_handler = true;
  device.hostile_hub = &f.hub;
  f.hub.register_device(&device);

  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.uart.inject_frame(0x02, READ_HOLDING_PDU);
  f.hub.loop();
  // The nested pass did dispatch the second frame, so the guard was really re-entered rather than left alone.
  EXPECT_EQ(device.reads, 2);
  EXPECT_FALSE(device.serviced_after_nested_loop);
}

}  // namespace esphome::modbus
