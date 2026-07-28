#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/hal.h"

namespace esphome::modbus::testing {

namespace {

// Exposes the frame state machine so tests can drive it without a UART: force_send_front()
// mimics send_next_frame_() transmitting the SELECTED (not storage-front) READY frame,
// timeout_waiting() mimics the loop() send-wait watchdog plus the sweeps around it, and
// sweep_for_test() runs the delivery sweep alone (the real one runs at the end of every loop()).
class NoResponseProbeHub : public ModbusClientHub {
 public:
  // The old "queue" view: entries awaiting transmission, in STORAGE order (selection order is
  // what the engine transmits by; use next_ready() for that).
  size_t queued_frames() const {
    size_t count = 0;
    for (const auto &cmd : this->tx_buffer_) {
      if (cmd.state == FrameState::READY)
        count++;
    }
    return count;
  }
  const ModbusDeviceCommand &queued(size_t i) const {
    for (const auto &cmd : this->tx_buffer_) {
      if (cmd.state == FrameState::READY && i-- == 0)
        return cmd;
    }
    ADD_FAILURE() << "no READY entry at that index";
    return this->tx_buffer_.front();
  }
  size_t entries() const { return this->tx_buffer_.size(); }
  const ModbusDeviceCommand *next_ready() { return this->select_next_ready_(); }
  bool waiting() const { return this->has_in_flight_; }
  const ModbusDeviceCommand &waiting_command() {
    ModbusDeviceCommand *cmd = this->find_in_flight_();
    EXPECT_NE(cmd, nullptr);
    return *cmd;
  }

  void sweep_for_test() { this->sweep_(); }
  void send_next_for_test() {
    this->send_next_frame_();
    this->sweep_();  // a transmit failure's on_not_sent() is delivered by the loop's sweep
  }
  void force_send_front() {
    ModbusDeviceCommand *cmd = this->select_next_ready_();
    ASSERT_NE(cmd, nullptr) << "no READY entry to send";
    cmd->state = FrameState::WAITING;
    this->has_in_flight_ = true;
    this->in_flight_address_ = cmd->frame.address();
  }
  // Drives the real response/interruption branches, followed by the loop's sweep.
  void receive_frame_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->process_modbus_server_frame(address, pdu);
    this->sweep_();
  }
  void timeout_waiting() {
    this->sweep_();  // deliver anything already owed (e.g. an interruption's on_no_response)
    this->expire_in_flight_();
    this->sweep_();
  }
};

// A device with a scripted answer to on_no_response().
class RetryingDevice : public ModbusClientDevice {
 public:
  RetryingDevice(ModbusClientHub *hub, uint8_t address, bool retry) : ModbusClientDevice(hub, address), retry_(retry) {}
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_count_++;
    return this->retry_;
  }
  int no_response_count_{0};

 protected:
  bool retry_{false};
};

// A device that clears its own queued traffic from inside the no-response callback, then asks for a retry.
class ClearingRetryDevice : public ModbusClientDevice {
 public:
  ClearingRetryDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_count_++;
    this->clear_tx_queue_for_device();  // detaches this device from the waiting slot mid-callback
    return true;                        // and still requests a retry
  }
  int no_response_count_{0};
};

constexpr uint8_t READ_PDU[] = {0x03, 0x01, 0x00, 0x00, 0x02};  // read 2 holding registers at 0x100

StaticVector<uint8_t, MAX_PDU_SIZE> read_pdu() {
  StaticVector<uint8_t, MAX_PDU_SIZE> pdu;
  pdu.assign(READ_PDU, READ_PDU + sizeof(READ_PDU));
  return pdu;
}

}  // namespace

// A device that requests a retry gets the frame the hub was holding re-queued on its behalf,
// byte-identical and still routed to the same device.
TEST(ModbusClientHubNoResponse, RetryRequeuesWaitingFrame) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  ASSERT_EQ(hub.queued_frames(), 1u);
  hub.force_send_front();
  ASSERT_EQ(hub.queued_frames(), 0u);
  ASSERT_TRUE(hub.waiting());

  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_FALSE(hub.waiting());
  ASSERT_EQ(hub.queued_frames(), 1u);
  const ModbusDeviceCommand &requeued = hub.queued(0);
  EXPECT_EQ(requeued.device, &device);
  // address + PDU + CRC
  ASSERT_EQ(requeued.frame.size(), sizeof(READ_PDU) + 3);
  EXPECT_EQ(requeued.frame.address(), 0x02);
  ASSERT_EQ(requeued.frame.pdu().size(), sizeof(READ_PDU));
  EXPECT_EQ(0, memcmp(requeued.frame.pdu().data(), READ_PDU, sizeof(READ_PDU)));
}

// A device that declines the retry has the frame dropped.
TEST(ModbusClientHubNoResponse, NoRetryDropsWaitingFrame) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.send_pdu(read_pdu());
  hub.force_send_front();

  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// After the device is detached from the waiting frame (e.g. clear_tx_queue_for_device on
// destruction), a timeout must not deliver a callback or re-queue anything.
TEST(ModbusClientHubNoResponse, DetachedDeviceIsNotNotified) {
  NoResponseProbeHub hub;
  {
    RetryingDevice device(&hub, 0x02, /*retry=*/true);
    device.send_pdu(read_pdu());
    hub.force_send_front();
    // device destructor clears its queue entries, including the waiting frame's device pointer
  }
  ASSERT_TRUE(hub.waiting());
  EXPECT_EQ(hub.waiting_command().device, nullptr);

  hub.timeout_waiting();

  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// An unexpected frame interrupts the transaction: the entry becomes an INTERRUPTED shell that
// keeps tx blocked until the send-wait timeout. The sweep delivers one on_no_response() and records
// the retry decision, which the timeout applies - without a second callback or a duplicate entry.
TEST(ModbusClientHubNoResponse, RetryBehindInterruptedShell) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();

  // A frame from the wrong address (0x07, expected 0x02) hits the unexpected-frame branch.
  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);

  hub.sweep_for_test();  // the loop's sweep delivers the interruption's on_no_response()

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(hub.queued_frames(), 0u);  // no separate requeue: the entry IS the shell
  ASSERT_TRUE(hub.waiting());          // and it keeps blocking the bus
  EXPECT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED_NOTIFIED);
  EXPECT_TRUE(hub.waiting_command().retry_after_interrupt);  // the retry decision is recorded...
  EXPECT_EQ(hub.waiting_command().device, &device);

  // ...and applied when the send-wait timeout releases the shell: one READY retry, no second callback.
  hub.timeout_waiting();
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(device.no_response_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).device, &device);
}

// A callback that detaches the device (clear_tx_queue_for_device()) wins over its own retry request:
// no orphaned frame with a null device is re-queued.
TEST(ModbusClientHubNoResponse, MidCallbackClearCancelsRetry) {
  NoResponseProbeHub hub;
  ClearingRetryDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(hub.queued_frames(), 0u);  // the retry was not re-queued for a detached device
  EXPECT_FALSE(hub.waiting());
}

// Writes jump ahead of queued reads; reads keep FIFO order among themselves.
TEST(ModbusClientHubPriority, WritesSendBeforeQueuedReads) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t read_a[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t read_b[] = {0x03, 0x02, 0x00, 0x00, 0x02};
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(read_a);
  device.send_pdu(read_b);
  device.send_pdu(write_pdu);

  ASSERT_EQ(hub.queued_frames(), 3u);
  hub.force_send_front();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[0], 0x06);  // the write transmits first
  hub.timeout_waiting();
  hub.force_send_front();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[1], 0x01);  // reads follow in FIFO order
  hub.timeout_waiting();
  hub.force_send_front();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[1], 0x02);
}

// Re-requesting a queued frame is absorbed into the existing entry instead of queueing a duplicate.
TEST(ModbusClientHubPriority, DuplicateQueuedFrameAbsorbedNotDuplicated) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.send_pdu(read_pdu());
  device.send_pdu(read_pdu());

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 2u);  // one entry standing for two accepted requests
}

// Re-requesting the frame currently in flight is absorbed into the waiting entry; after a
// no-response timeout the absorbed request still gets its run even though the device declines a
// retry, and a second timeout does not run it again.
TEST(ModbusClientHubPriority, InFlightDuplicateRunsOnceMore) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // duplicate of the in-flight frame

  EXPECT_EQ(hub.queued_frames(), 0u);  // not queued twice
  EXPECT_EQ(hub.waiting_command().pending, 2u);

  hub.timeout_waiting();
  ASSERT_EQ(hub.queued_frames(), 1u);  // the timeout resolved one request; the absorbed one runs
  EXPECT_EQ(hub.queued(0).pending, 1u);

  hub.force_send_front();
  hub.timeout_waiting();
  EXPECT_EQ(hub.queued_frames(), 0u);  // the last request resolved; nothing left to run
}

// An entry with an absorbed extra request that times out while the device asks to retry: the
// retry is not a resolution, so BOTH requests remain pending rather than one being dropped -
// which would leave that caller without a resolution.
TEST(ModbusClientHubPriority, AbsorbedRequestSurvivesDeviceRetry) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // duplicate of the in-flight frame -> absorbed
  ASSERT_EQ(hub.waiting_command().pending, 2u);

  hub.timeout_waiting();  // no response; the device requests a retry

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 2u);  // preserved: the retry resolved nothing
}

// A continuous read re-queues itself (at the lowest priority) after each successful response,
// but not after an exception response.
TEST(ModbusClientHubPriority, ContinuousReadRequeuesOnSuccessOnly) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);
  hub.force_send_front();

  // A matching successful response cycles the resident entry back to READY.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);

  // An exception response ends the poll.
  hub.force_send_front();
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A continuous read that gets no response and is retried stays continuous: an explicit retry of a
// continuous poll is assumed to still want continuous polling (the entry stays resident).
TEST(ModbusClientHubPriority, RetriedContinuousReadStaysContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_TRUE(hub.queued(0).resident);
  hub.force_send_front();

  hub.timeout_waiting();  // no response -> device requests retry

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);  // the retried poll stays resident
}

// A duplicate send never converts a resident entry back to a one-shot: polling would silently
// stop. The duplicate request is served, uncounted, by the poll's next response instead.
TEST(ModbusClientHubPriority, DuplicateSendKeepsContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  device.read_holding_registers(0x100, 2);  // queued duplicate: absorbed uncounted
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);
  EXPECT_EQ(hub.queued(0).pending, 1u);

  hub.force_send_front();
  device.read_holding_registers(0x100, 2);  // in-flight duplicate: absorbed uncounted
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_TRUE(hub.waiting_command().resident);
  EXPECT_EQ(hub.waiting_command().pending, 1u);

  // The poll survives the duplicates: a successful response still cycles it back to READY.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);
}

// Requesting continuous polling for a frame that is already queued as a one-shot turns that entry
// into the continuous poll instead of leaving a promotion that never polls.
TEST(ModbusClientHubPriority, ContinuousRequestUpgradesQueuedDuplicate) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2);
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_FALSE(hub.queued(0).resident);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);

  // And it behaves as a poll from here: success cycles it back to READY.
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).resident);
}

// continuous is ignored for writes: the frame still sends at WRITE priority, once.
TEST(ModbusClientHubPriority, ContinuousIgnoredForWrites) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);
  EXPECT_FALSE(hub.queued(0).resident);
}

// A device whose sent/not-sent callbacks are counted.
namespace {
class SentCountingDevice : public ModbusClientDevice {
 public:
  SentCountingDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_sent(std::span<const uint8_t> request_pdu) override {
    this->sent_count_++;
    this->last_sent_pdu_.assign(request_pdu.begin(), request_pdu.end());
  }
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->last_not_sent_pdu_.assign(request_pdu.begin(), request_pdu.end());
  }
  int sent_count_{0};
  int not_sent_count_{0};
  std::vector<uint8_t> last_sent_pdu_;
  std::vector<uint8_t> last_not_sent_pdu_;
};
}  // namespace

// A write is never requeueable: a duplicate of a queued write is bled off with on_not_sent() at
// the next sweep rather than earning the write an extra transmission.
TEST(ModbusClientHubPriority, DuplicateQueuedWriteDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(write_pdu);  // duplicate write
  hub.sweep_for_test();        // the loop's sweep bleeds the over-cap request

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // bled back to a write's cap of one
  EXPECT_EQ(device.not_sent_count_, 1);  // the duplicate was dropped
}

// Requeueability is an allow-list of the standard reads: a custom function code's idempotency is
// unknown, so its duplicate takes the same safe drop path as a write instead of a silent re-send.
TEST(ModbusClientHubPriority, DuplicateCustomFunctionCodeDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t custom_pdu[] = {0x41, 0x01, 0x02};  // user-defined function code
  device.send_pdu(custom_pdu);
  device.send_pdu(custom_pdu);  // duplicate custom command
  hub.sweep_for_test();

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // bled to the non-requeueable cap of one run
  EXPECT_EQ(device.not_sent_count_, 1);  // the duplicate was dropped
}

// An anonymous duplicate (no device - the YAML-lambda path) is always dropped, never promoted:
// with no callback there is no lifecycle to absorb into and no owner to route a re-run to.
TEST(ModbusClientHubPriority, AnonymousDuplicateDroppedNotPromoted) {
  NoResponseProbeHub hub;

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  hub.send_pdu(0x02, read);
  hub.send_pdu(0x02, read);  // anonymous duplicate: dropped

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // never absorbed for a null owner
}

// A retried entry is re-stamped to the queue tail: reads that arrived while it was in flight get
// their turn before the retry, so a frame that keeps timing out cannot starve the rest of the bus.
TEST(ModbusClientHubPriority, RetriedReadGoesBehindFreshReads) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();       // the frame that will time out and retry
  device.send_pdu(read_pdu());  // in-flight duplicate: absorbed into the waiting entry
  const uint8_t fresh_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t fresh_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  device.send_pdu(fresh_a);
  device.send_pdu(fresh_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.timeout_waiting();  // device retries; the entry returns to READY behind the fresh reads

  ASSERT_EQ(hub.queued_frames(), 3u);
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.pdu()[2], 0x10);  // fresh reads keep FIFO order ahead of the retry
  hub.force_send_front();
  hub.timeout_waiting();
  hub.force_send_front();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[2], 0x20);
  hub.timeout_waiting();
  hub.force_send_front();  // the retry gets its turn last, both requests still on the entry
  EXPECT_TRUE(std::equal(hub.waiting_command().frame.pdu().begin(), hub.waiting_command().frame.pdu().end(), READ_PDU));
  EXPECT_EQ(hub.waiting_command().pending, 2u);
}

// An absorbed duplicate does not move the entry back in line: seq belongs to the entry, and only
// re-entering the line (retry, resolved request, resident cycle) re-stamps it.
TEST(ModbusClientHubPriority, AbsorbedDuplicateKeepsPlaceInLine) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t read_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  device.send_pdu(read_a);
  device.send_pdu(read_b);
  device.send_pdu(read_a);  // duplicate of the older entry: absorbed, place unchanged
  ASSERT_EQ(hub.queued_frames(), 2u);

  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.pdu()[2], 0x10);  // read_a still transmits first
  EXPECT_EQ(next->pending, 2u);
}

// A write that is retried after a no-response keeps the WRITE class, so it stays ahead of reads,
// and a later duplicate still resolves against it instead of queueing twice.
TEST(ModbusClientHubPriority, RetriedWriteKeepsWritePriorityAndStaysNonRequeueable) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  hub.force_send_front();
  hub.timeout_waiting();  // no response -> device requests retry -> back to READY

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);  // retry preserves the WRITE class

  device.send_pdu(write_pdu);          // duplicate of the retried write
  ASSERT_EQ(hub.queued_frames(), 1u);  // still not queued twice...
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);
  hub.sweep_for_test();
  EXPECT_EQ(hub.queued(0).pending, 1u);  // ...and the duplicate bled off at the sweep
}

// on_sent() fires when the frame goes onto the wire, not when it is queued.
TEST(ModbusClientHubSent, FiresOnWireNotOnQueue) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();  // frame timing derives from the baud rate
  SentCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  EXPECT_EQ(device.sent_count_, 0);  // queued only - nothing on the wire yet

  hub.send_next_for_test();
  EXPECT_EQ(device.sent_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 0);
  // The callback identifies which command transmitted: it carries the request PDU.
  EXPECT_EQ(device.last_sent_pdu_, (std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU))));
  EXPECT_TRUE(hub.waiting());
}

// Counts response deliveries so requeue semantics can be pinned end to end.
namespace {
class DataCountingDevice : public ModbusClientDevice {
 public:
  DataCountingDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->data_count_++;
  }
  void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) override { this->error_count_++; }
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_count_++;
    this->last_no_response_pdu_.assign(request_pdu.begin(), request_pdu.end());
    if (this->retries_ == 0)
      return false;
    this->retries_--;
    return true;
  }
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->last_not_sent_pdu_.assign(request_pdu.begin(), request_pdu.end());
  }
  void on_sent(std::span<const uint8_t> request_pdu) override { this->sent_count_++; }
  int terminals() const {
    return this->data_count_ + this->error_count_ + this->no_response_count_ + this->not_sent_count_;
  }
  int data_count_{0};
  int error_count_{0};
  int no_response_count_{0};
  int not_sent_count_{0};
  int sent_count_{0};
  int retries_{0};
  std::vector<uint8_t> last_not_sent_pdu_;
  std::vector<uint8_t> last_no_response_pdu_;
};

// Runs full send/respond cycles until the queue drains; returns the number of cycles executed.
int drain_with_responses(NoResponseProbeHub &hub, std::span<const uint8_t> response_pdu, int max_cycles = 10) {
  int cycles = 0;
  while (hub.queued_frames() != 0 && cycles < max_cycles) {
    hub.force_send_front();
    hub.receive_frame_for_test(0x02, response_pdu);
    cycles++;
  }
  return cycles;
}
}  // namespace

constexpr uint8_t OK_RESPONSE[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};

// One request produces exactly one data callback.
TEST(ModbusClientHubCallbackCount, SingleReadSingleCallback) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  drain_with_responses(hub, OK_RESPONSE);

  EXPECT_EQ(device.data_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 0);
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_FALSE(hub.waiting());
}

// Requesting the same read twice while queued yields exactly two callbacks:
// the promoted entry completes, re-queues once (demoted), completes again, and stops.
TEST(ModbusClientHubCallbackCount, DuplicateReadExactlyTwoCallbacks) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  device.send_pdu(read_pdu());
  int cycles = drain_with_responses(hub, OK_RESPONSE);

  EXPECT_EQ(cycles, 2);
  EXPECT_EQ(device.data_count_, 2);
  EXPECT_EQ(device.not_sent_count_, 0);  // both requests were served
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// Every request resolves in exactly one callback: three identical requests yield two reads
// (the promoted entry runs once more) plus one on_not_sent() for the dropped third.
TEST(ModbusClientHubCallbackCount, TripleReadStillTwoCallbacks) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  device.send_pdu(read_pdu());
  device.send_pdu(read_pdu());
  hub.sweep_for_test();                  // the sweep bleeds the over-cap third request
  EXPECT_EQ(device.not_sent_count_, 1);  // one refusal terminal for it
  EXPECT_EQ(device.last_not_sent_pdu_,   // and the terminal identifies which request was refused
            (std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU))));
  int cycles = drain_with_responses(hub, OK_RESPONSE);

  EXPECT_EQ(cycles, 2);
  EXPECT_EQ(device.data_count_, 2);
  EXPECT_EQ(device.not_sent_count_, 1);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A duplicate write cannot be promoted (that would demote its priority); it is dropped
// with on_not_sent() and the original write sends once.
TEST(ModbusClientHubCallbackCount, DuplicateWriteDroppedWithNotSent) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(write_pdu);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);
}

// An exception response is a terminal on its own: exactly one on_error(), no others,
// preceded by exactly one on_sent().
TEST(ModbusClientHubCallbackCount, ErrorResponseIsSoleTerminal) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.send_next_for_test();
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);

  EXPECT_EQ(device.error_count_, 1);
  EXPECT_EQ(device.terminals(), 1);
  EXPECT_EQ(device.sent_count_, 1);
}

// A timeout is a terminal on its own: exactly one on_no_response(), preceded by one
// on_sent(); a refused duplicate ends in on_not_sent() with NO on_sent().
TEST(ModbusClientHubCallbackCount, NoResponseIsSoleTerminalAndNotSentHasNoSent) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.send_next_for_test();
  hub.timeout_waiting();
  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.terminals(), 1);
  EXPECT_EQ(device.sent_count_, 1);

  // Unabsorbable duplicate: two identical writes -> the second is a not_sent terminal, never sent.
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(write_pdu);
  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 1);
  EXPECT_EQ(device.terminals(), 2);  // the accepted write is still queued - no terminal for it yet
  EXPECT_EQ(device.sent_count_, 1);  // and it has not transmitted yet

  // Drain it: the write echo response is its data terminal, and the books balance.
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, write_pdu);
  EXPECT_EQ(device.data_count_, 1);
  EXPECT_EQ(device.terminals(), 3);  // 3 accepted lifecycles, 3 terminals
  EXPECT_EQ(device.sent_count_, 2);  // 2 transmissions (read + write); the refused duplicate never sent
}

// A device-requested retry starts a new lifecycle: each transmission gets its own sent + terminal.
TEST(ModbusClientHubCallbackCount, RetryLifecyclesEachGetSentAndTerminal) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);
  device.retries_ = 1;  // ask for exactly one retry

  device.send_pdu(read_pdu());
  hub.send_next_for_test();
  hub.timeout_waiting();  // lifecycle 1: sent + no_response (retry requested -> re-queued)
  ASSERT_EQ(hub.queued_frames(), 1u);
  hub.send_next_for_test();
  hub.timeout_waiting();  // lifecycle 2: sent + no_response (retry declined -> done)

  EXPECT_EQ(device.no_response_count_, 2);
  EXPECT_EQ(device.terminals(), 2);
  EXPECT_EQ(device.sent_count_, 2);
  EXPECT_EQ(hub.queued_frames(), 0u);
  // The retried lifecycle's timeout carries the SAME request PDU as the first attempt.
  EXPECT_EQ(device.last_no_response_pdu_, std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU)));
}

// Under the state machine a retry can NEVER be refused by a full queue: the entry already lives
// in the container and a retry is a state flip, not a new insertion. Fill the queue, time out the
// in-flight frame with a retry - the frame survives as READY and both absorbed requests persist.
// (The old requeue_waiting_frame_/maybe_requeue_completed_ full-buffer refusal branches, and their
// three tests, are gone with the branches themselves.)
TEST(ModbusClientHubCallbackCount, RetryIsNeverRefusedByFullQueue) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);
  device.retries_ = 1;
  SentCountingDevice filler(&hub, 0x05);

  device.send_pdu(read_pdu());
  hub.force_send_front();       // in flight
  device.send_pdu(read_pdu());  // absorbed: two requests pending
  // Fill the remaining live capacity with distinct frames.
  for (uint16_t i = 0; hub.entries() < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }

  hub.timeout_waiting();  // retry requested; the entry flips back to READY regardless of capacity

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 0);  // nothing was refused
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->device, &filler);  // round-robin: the retry re-stamped behind the fillers
  // The retried entry survives as READY with both absorbed requests intact.
  bool found = false;
  for (size_t i = 0; i < hub.queued_frames(); i++) {
    const ModbusDeviceCommand &cmd = hub.queued(i);
    if (cmd.device == &device) {
      EXPECT_EQ(cmd.pending, 2u);
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

// The deprecated device-side send_raw() refusal delivers through the same guard as every other
// path: a handler that reacts to its own refusal with another empty send_raw() stays bounded.
namespace {
class SendRawOnNotSentDevice : public ModbusClientDevice {
 public:
  SendRawOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    this->send_raw({});  // refused again; the guard must suppress the nested delivery
#pragma GCC diagnostic pop
  }
  int not_sent_count_{0};
};
}  // namespace

TEST(ModbusClientHubQueue, SendRawRefusalIsGuardedAgainstRecursion) {
  NoResponseProbeHub hub;
  SendRawOnNotSentDevice device(&hub, 0x02);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  device.send_raw({});  // empty payload refused -> on_not_sent -> nested send_raw({}) suppressed
#pragma GCC diagnostic pop
  EXPECT_EQ(device.not_sent_count_, 1);
}

// A continuous read: every wire transmission pairs one sent with one terminal, ending on the error.
TEST(ModbusClientHubCallbackCount, ContinuousLifecyclesBalance) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, ok_response);  // lifecycle 1 -> requeued
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, ok_response);  // lifecycle 2 -> requeued
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, exception_response);  // lifecycle 3 -> stops

  EXPECT_EQ(device.data_count_, 2);
  EXPECT_EQ(device.error_count_, 1);
  EXPECT_EQ(device.terminals(), 3);
  EXPECT_EQ(device.sent_count_, 3);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// A device that stops itself (clears its own queue) from inside on_response().
class ClearOnDataDevice : public ModbusClientDevice {
 public:
  ClearOnDataDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->clear_tx_queue_for_device();
  }
};

// A device that chains a follow-up send from inside on_sent().
class ChainOnSentDevice : public ModbusClientDevice {
 public:
  ChainOnSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_sent(std::span<const uint8_t> request_pdu) override {
    if (!this->chained_) {
      this->chained_ = true;
      const uint8_t follow[] = {0x03, 0x00, 0x09, 0x00, 0x01};  // read holding 0x0009 x1
      this->send_pdu(follow);
    }
  }
  bool chained_{false};
};
}  // namespace

// clear_tx_queue_for_address() resolves every dropped frame via its owner's on_not_sent(), so a device
// sharing the address with the clearer (e.g. a modbus_client action alongside an offline controller)
// observes the drop; frames for other addresses are untouched.
TEST(ModbusClientHubQueue, ClearAddressQueueNotifiesEveryOwner) {
  NoResponseProbeHub hub;
  SentCountingDevice controller_like(&hub, 0x02);
  SentCountingDevice bystander_same(&hub, 0x02);
  SentCountingDevice bystander_other(&hub, 0x03);

  const uint8_t read_a[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t read_b[] = {0x03, 0x02, 0x00, 0x00, 0x02};
  const uint8_t read_c[] = {0x03, 0x03, 0x00, 0x00, 0x02};
  controller_like.send_pdu(read_a);
  bystander_same.send_pdu(read_b);
  bystander_other.send_pdu(read_c);
  ASSERT_EQ(hub.queued_frames(), 3u);

  controller_like.clear_tx_queue_for_address(false);
  hub.sweep_for_test();  // the loop's sweep delivers the owed terminals and erases the entries

  ASSERT_EQ(hub.queued_frames(), 1u);  // only the other-address frame remains
  EXPECT_EQ(hub.queued(0).frame.address(), 0x03);
  EXPECT_EQ(controller_like.not_sent_count_, 1);
  EXPECT_EQ(bystander_same.not_sent_count_, 1);
  EXPECT_EQ(bystander_other.not_sent_count_, 0);
  // each owner saw its own request PDU
  EXPECT_EQ(bystander_same.last_not_sent_pdu_, std::vector<uint8_t>(std::begin(read_b), std::end(read_b)));
}

// A cleared entry resolves with one on_not_sent() per accepted request it stood for, so the
// books balance for owners counting outstanding requests.
TEST(ModbusClientHubQueue, ClearAddressDeliversOneTerminalPerAcceptedRequest) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  device.send_pdu(read);
  device.send_pdu(read);  // duplicate: absorbed into the queued entry
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.queued(0).pending, 2u);

  hub.clear_tx_queue_for_address(0x02, false);
  hub.sweep_for_test();

  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(device.not_sent_count_, 2);  // one terminal per accepted request
}

namespace {
// Re-sends its frame once from inside on_not_sent - the re-queued frame must survive the sweep.
class ResendOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t again[] = {0x06, 0x00, 0x40, 0x00, 0x01};  // a write: priority-inserts at the front
      this->send_pdu(again);
    }
  }
  int not_sent_count_{0};
};
}  // namespace

// A handler that re-sends to the same address from inside on_not_sent() neither corrupts the sweep
// nor loops it: the fresh entry starts within its cap, so the sweep never touches it.
TEST(ModbusClientHubQueue, ClearAddressReentrantResendSurvives) {
  NoResponseProbeHub hub;
  ResendOnNotSentDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.clear_tx_queue_for_address(0x02, false);
  hub.sweep_for_test();

  // The original frame resolved via on_not_sent; the re-send from inside that callback remains queued.
  EXPECT_EQ(device.not_sent_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).frame.address(), 0x02);
}

// The hard case for the sweep: the notified handler re-queues a WRITE to the cleared address. The
// fresh entry must be neither dropped nor re-notified - and the bystander's frame at the other
// address survives untouched, while the write still wins transmit selection.
TEST(ModbusClientHubQueue, ClearAddressReentrantResendNotSwept) {
  NoResponseProbeHub hub;
  ResendOnNotSentDevice resender(&hub, 0x02);
  SentCountingDevice bystander_other(&hub, 0x03);

  const uint8_t read_victim[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_other[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  resender.send_pdu(read_victim);
  bystander_other.send_pdu(read_other);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02, false);
  hub.sweep_for_test();

  EXPECT_EQ(resender.not_sent_count_, 1);  // notified once, never re-notified for the re-send
  ASSERT_EQ(hub.queued_frames(), 2u);      // the re-queued write AND the other-address read survive
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.address(), 0x02);  // the WRITE class wins selection over the older read
  EXPECT_EQ(next->frame.pdu()[0], 0x06);
}

namespace {
// Retries from EVERY on_not_sent - against a full queue this recursed without bound before the guard.
class AlwaysRetryDevice : public ModbusClientDevice {
 public:
  AlwaysRetryDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    const uint8_t again[] = {0x03, 0x00, 0x50, 0x00, 0x01};
    this->send_pdu(again);
  }
  int not_sent_count_{0};
};

// From inside on_not_sent, clears ANOTHER address - those victims must still be notified (the per-device
// guard suppresses deliveries only to a device already inside its own on_not_sent()).
class ClearOtherOnNotSentDevice : public ModbusClientDevice {
 public:
  ClearOtherOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->parent_->clear_tx_queue_for_address(0x03, false);
  }
  int not_sent_count_{0};
};
}  // namespace

// A handler that retries from every on_not_sent() against a FULL queue must not livelock the
// sweep: one refusal delivery per device per sweep - the repeat is dropped without a callback.
TEST(ModbusClientHubQueue, FullQueueRetryFromNotSentDoesNotRecurse) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  AlwaysRetryDevice retrier(&hub, 0x02);

  // Fill the queue with distinct frames (distinct start addresses keep the dedup from absorbing them).
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  retrier.send_pdu(read);  // refused (full); the terminal is owed to the sweep
  hub.sweep_for_test();    // delivers it; the handler's retry-refusal is suppressed for this sweep

  EXPECT_EQ(retrier.not_sent_count_, 1);
  EXPECT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);
}

namespace {
// From inside on_not_sent, triggers ANOTHER device's send (which will be refused too).
class SendOtherOnNotSentDevice : public ModbusClientDevice {
 public:
  SendOtherOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->other_ != nullptr) {
      const uint8_t read[] = {0x03, 0x00, 0x60, 0x00, 0x01};
      this->other_->send_pdu(read);
    }
  }
  ModbusClientDevice *other_{nullptr};
  int not_sent_count_{0};
};
}  // namespace

// The refusal suppression is per-device: a refusal that lands on a DIFFERENT device during the
// same sweep must still deliver - that device did not cause the loop and would otherwise silently
// lose its terminal callback.
TEST(ModbusClientHubQueue, RefusalForOtherDeviceDeliversDuringNotification) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  SendOtherOnNotSentDevice first(&hub, 0x02);
  SentCountingDevice second(&hub, 0x03);
  first.other_ = &second;

  // Fill the queue with distinct frames (distinct start addresses keep the dedup from absorbing them).
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  first.send_pdu(read);  // refused; the sweep notifies first, whose handler gets second refused too
  hub.sweep_for_test();

  EXPECT_EQ(first.not_sent_count_, 1);
  EXPECT_EQ(second.not_sent_count_, 1);
}

// Two devices whose handlers each trigger the other's send cannot loop the sweep without bound:
// each device receives at most one refusal delivery per sweep, so the cycle dies on its second
// visit to either device.
TEST(ModbusClientHubQueue, TwoDeviceRefusalCycleTerminates) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  SendOtherOnNotSentDevice first(&hub, 0x02);
  SendOtherOnNotSentDevice second(&hub, 0x03);
  first.other_ = &second;
  second.other_ = &first;

  // Fill the queue with distinct frames (distinct start addresses keep the dedup from absorbing them).
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  first.send_pdu(read);  // refuse -> first -> second refused -> second -> first suppressed -> done
  hub.sweep_for_test();

  EXPECT_EQ(first.not_sent_count_, 1);
  EXPECT_EQ(second.not_sent_count_, 1);
}

namespace {
// From inside on_not_sent, clears its OWN address - its remaining queued frames resolve silently
// (the guard suppresses self-deliveries), while other owners on the address are still notified.
class ClearOwnAddressOnNotSentDevice : public ModbusClientDevice {
 public:
  ClearOwnAddressOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->clear_tx_queue_for_address(/*clear_sent=*/false);
  }
  int not_sent_count_{0};
};
}  // namespace

// An address clear issued from inside on_not_sent() resolves EVERY dropped request with its own
// terminal at the sweep - including the clearer's (the sweep delivers from a quiescent hub, so the
// old stack-nesting silence no longer applies; use clear_tx_queue_for_device() for silent teardown).
TEST(ModbusClientHubQueue, SelfClearFromNotSentResolvesEveryRequest) {
  NoResponseProbeHub hub;
  ClearOwnAddressOnNotSentDevice clearer(&hub, 0x02);
  SentCountingDevice bystander(&hub, 0x02);

  const uint8_t read_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  const uint8_t read_c[] = {0x03, 0x00, 0x30, 0x00, 0x01};
  clearer.send_pdu(read_a);
  clearer.send_pdu(read_b);
  bystander.send_pdu(read_c);
  ASSERT_EQ(hub.queued_frames(), 3u);

  clearer.send_pdu(std::span<const uint8_t>{});  // refused inline (empty) -> the handler clears the address
  hub.sweep_for_test();

  EXPECT_EQ(clearer.not_sent_count_, 3);    // the refusal plus both cleared requests
  EXPECT_EQ(bystander.not_sent_count_, 1);  // the bystander's cleared frame is notified too
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// The suppression must not over-reach: a clear issued from inside on_not_sent() still delivers
// its victims' notifications in the same sweep (only repeat refusals to one device are silenced).
TEST(ModbusClientHubQueue, NestedClearFromNotSentStillNotifiesVictims) {
  NoResponseProbeHub hub;
  ClearOtherOnNotSentDevice clearer(&hub, 0x02);
  SentCountingDevice victim(&hub, 0x03);

  const uint8_t read_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  clearer.send_pdu(read_a);
  victim.send_pdu(read_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02, false);  // clearer's on_not_sent clears address 0x03 in turn
  hub.sweep_for_test();

  EXPECT_EQ(clearer.not_sent_count_, 1);
  EXPECT_EQ(victim.not_sent_count_, 1);  // the nested clear's victim resolves in the same sweep
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// tx_blocked() flips to blocked after the first check, so send_next_frame_() passes its own gate but
// send_frame_() refuses - a deterministic transmit failure.
class FlakyBlockHub : public NoResponseProbeHub {
 public:
  bool tx_blocked() override {
    this->tx_blocked_calls_++;
    return this->tx_blocked_calls_ > 1;
  }
  int tx_blocked_calls_{0};
};

// Reacts to a transmit failure by sending a WRITE (which then wins transmit selection).
class WriteOnNotSentDevice : public ModbusClientDevice {
 public:
  WriteOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    const uint8_t write[] = {0x06, 0x00, 0x40, 0x01, 0x02};
    this->send_pdu(write);
  }
  int not_sent_count_{0};
};

// From on_not_sent (delivered by the sweep), re-sends a frame identical to ANOTHER doomed queued
// frame; the dedup must not absorb into the doomed entry.
class ResendSecondFrameDevice : public ModbusClientDevice {
 public:
  ResendSecondFrameDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t same_as_r2[] = {0x03, 0x00, 0x22, 0x00, 0x01};
      this->send_pdu(same_as_r2);
    }
  }
  int not_sent_count_{0};
};
}  // namespace

// A transmit failure resolves through the sweep with the failed entry already REFUSED: a handler
// that reacts by sending a write gets a fresh entry that survives, and the failed read is gone.
TEST(ModbusClientHubQueue, TransmitFailureHandlerResendSurvives) {
  FlakyBlockHub hub;
  WriteOnNotSentDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.send_next_for_test();  // tx_blocked gate passes, send_frame_ refuses -> failure + sweep

  EXPECT_EQ(device.not_sent_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);             // the handler's write survives...
  EXPECT_EQ(hub.queued(0).frame.pdu()[0], 0x06);  // ...and it is the write, not the failed read
}

// A failed transmit of an entry with an absorbed extra request resolves ONE request (the failed
// attempt's on_not_sent) and returns the entry to READY for the absorbed request - the same books
// as the timeout path, because a transmit failure is transient, unlike a clear's cancellation.
TEST(ModbusClientHubQueue, TransmitFailureResolvesOneAndKeepsAbsorbedRequest) {
  FlakyBlockHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  device.send_pdu(read);  // absorbed into the queued entry
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.queued(0).pending, 2u);

  hub.send_next_for_test();  // tx_blocked gate passes, send_frame_ refuses -> failure + sweep

  EXPECT_EQ(device.not_sent_count_, 1);  // the failed attempt's terminal
  ASSERT_EQ(hub.queued_frames(), 1u);    // the absorbed request still owes a run
  EXPECT_EQ(hub.queued(0).pending, 1u);
  EXPECT_TRUE(std::equal(hub.queued(0).frame.pdu().begin(), hub.queued(0).frame.pdu().end(), read));
}

// A send during a sweep that matches a DELETED (doomed) frame must queue fresh, not absorb into
// the doomed entry - absorption would tie the new request to a frame the sweep is draining.
TEST(ModbusClientHubQueue, SweepDedupSkipsDeletedFrames) {
  NoResponseProbeHub hub;
  ResendSecondFrameDevice device(&hub, 0x02);

  const uint8_t r1[] = {0x03, 0x00, 0x21, 0x00, 0x01};
  const uint8_t r2[] = {0x03, 0x00, 0x22, 0x00, 0x01};
  device.send_pdu(r1);
  device.send_pdu(r2);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02, false);
  hub.sweep_for_test();
  // r1's notification re-sent a frame identical to the DELETED r2. Without the dedup's dead-state
  // skip it would be absorbed into r2 and drained with it; with the skip it queues fresh and survives.

  EXPECT_EQ(device.not_sent_count_, 2);  // r1 and r2 both resolved
  ASSERT_EQ(hub.queued_frames(), 1u);    // the re-send survives
  EXPECT_EQ(hub.queued(0).frame.pdu()[2], 0x22);
}

// clear_tx_queue_for_device() drops queued frames SILENTLY - no terminal callback (the documented
// exception to the exactly-one-terminal contract; used during teardown/offline handling).
TEST(ModbusClientHubQueue, ClearDeviceQueueDropsSilently) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read_a[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t read_b[] = {0x03, 0x02, 0x00, 0x00, 0x02};
  device.send_pdu(read_a);
  device.send_pdu(read_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  device.clear_tx_queue_for_device();

  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(device.not_sent_count_, 0);  // silent drop: no terminal callback
}

// A send_pdu() from inside on_sent() enqueues behind the in-flight frame rather than sending
// immediately or corrupting the in-flight transaction.
TEST(ModbusClientHubSent, ReentrantSendFromOnSentQueues) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  ChainOnSentDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.send_next_for_test();  // first frame goes on the wire -> on_sent chains a follow-up

  EXPECT_TRUE(hub.waiting());                     // first frame is in flight
  ASSERT_EQ(hub.queued_frames(), 1u);             // the follow-up queued behind it, not sent
  EXPECT_EQ(hub.queued(0).frame.pdu()[2], 0x09);  // it is the chained read (start address 0x0009)
}

// "Stop polling now" from inside on_response() works: the completing command is exposed to the
// clear routines, which detach it, cancelling the pending continuous re-queue.
TEST(ModbusClientHubPriority, ClearDeviceDuringDataCancelsContinuousRequeue) {
  NoResponseProbeHub hub;
  ClearOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);

  // "Stop polling now" from inside on_response() works: the completing command is detached, so the
  // continuous re-queue is cancelled.
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// A device that stops polling for its address (clear by address) from inside on_response().
class ClearAddressOnDataDevice : public ModbusClientDevice {
 public:
  ClearAddressOnDataDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->clear_tx_queue_for_address(/*clear_sent=*/false);
  }
};
}  // namespace

// The address-scoped clear cancels the mid-completion re-queue the same way the device-scoped one does.
TEST(ModbusClientHubPriority, ClearAddressDuringDataCancelsContinuousRequeue) {
  NoResponseProbeHub hub;
  ClearAddressOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);

  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// Overrides only the DEPRECATED on_modbus_* names: the new-name default implementations must forward, so
// external devices written against the old names keep working through the deprecation window.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
class LegacyNameDevice : public ModbusClientDevice {
 public:
  LegacyNameDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_modbus_not_sent() override { this->legacy_not_sent_++; }
  bool on_modbus_no_response() override {
    this->legacy_no_response_++;
    return false;
  }
  int legacy_not_sent_{0};
  int legacy_no_response_{0};
};
#pragma GCC diagnostic pop
}  // namespace

TEST(ModbusClientHubCompat, LegacyCallbackNamesStillForward) {
  NoResponseProbeHub hub;
  LegacyNameDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  hub.force_send_front();
  hub.timeout_waiting();  // no reply -> on_no_response -> forwards to on_modbus_no_response
  EXPECT_EQ(device.legacy_no_response_, 1);

  device.send_pdu(std::span<const uint8_t>());  // empty PDU refused -> on_not_sent -> forwards
  EXPECT_EQ(device.legacy_not_sent_, 1);
}

// The send_pdu() capacity bound: a PDU larger than MAX_PDU_SIZE would build a frame past the RTU
// 256-byte limit, so it is refused up front and signalled like any other failed send.
TEST(ModbusClientHub, OversizedPduIsRefusedWithNotSent) {
  NoResponseProbeHub hub;
  LegacyNameDevice device(&hub, 0x02);
  std::vector<uint8_t> big(MAX_PDU_SIZE + 1, 0x41);
  device.send_pdu(big);
  EXPECT_EQ(device.legacy_not_sent_, 1);  // on_not_sent, observed via the legacy forward
  EXPECT_TRUE(hub.tx_buffer_empty());
}

// --- ModbusDevice compatibility shim ------------------------------------------------------------
// External components written against the pre-2026.8 API subclass ModbusDevice and override the
// old callbacks; the shim adapts the span-based hooks back to those signatures.
namespace {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
class LegacyApiDevice : public ModbusDevice {
 public:
  LegacyApiDevice(ModbusClientHub *hub, uint8_t address) : ModbusDevice(hub, address) {}
  void on_modbus_data(const std::vector<uint8_t> &data) override { this->last_data_ = data; }
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override {
    this->last_error_fc_ = function_code;
    this->last_error_code_ = exception_code;
  }
  std::vector<uint8_t> last_data_;
  int last_error_fc_{-1};
  int last_error_code_{-1};
};
#pragma GCC diagnostic pop
}  // namespace

TEST(ModbusDeviceShim, LegacyCallbacksReceiveTheOldShapes) {
  NoResponseProbeHub hub;
  LegacyApiDevice device(&hub, 0x02);

  // Read response: on_modbus_data() historically received the payload after the function code and
  // the byte-count byte, as an owning vector.
  const uint8_t read_req[] = {0x03, 0x00, 0x10, 0x00, 0x02};
  device.send_pdu(read_req);
  hub.force_send_front();
  const uint8_t response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, response);
  const std::vector<uint8_t> expected{0x00, 0x2A, 0x01, 0x00};
  EXPECT_EQ(device.last_data_, expected);

  // Write echo: no byte-count byte, so the payload is everything after the function code.
  const uint8_t write_req[] = {0x06, 0x00, 0x10, 0x00, 0x2A};
  device.send_pdu(write_req);
  hub.force_send_front();
  hub.receive_frame_for_test(0x02, write_req);  // single-write responses echo the request
  const std::vector<uint8_t> expected_echo{0x00, 0x10, 0x00, 0x2A};
  EXPECT_EQ(device.last_data_, expected_echo);

  // Exception response: on_modbus_error() received the masked function code and the exception code.
  device.send_pdu(read_req);
  hub.force_send_front();
  const uint8_t error[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, error);
  EXPECT_EQ(device.last_error_fc_, 0x03);
  EXPECT_EQ(device.last_error_code_, 0x02);
}

// --- typed send helpers --------------------------------------------------------------------------
// Each helper is a one-line forward onto a merged builder; these pin the function code and wire
// bytes each one queues, so a swapped code or transposed field cannot survive review silently.
TEST(ModbusTypedSendHelpers, HelpersQueueExpectedPdus) {
  NoResponseProbeHub hub;
  ModbusClientDevice device(&hub, 0x02);
  auto check = [&](const std::vector<uint8_t> &expected) {
    ASSERT_EQ(hub.queued_frames(), 1u);
    auto pdu = hub.queued(0).frame.pdu();
    EXPECT_EQ(std::vector<uint8_t>(pdu.begin(), pdu.end()), expected);
    hub.force_send_front();
    hub.timeout_waiting();  // default on_no_response() declines the retry, dropping the frame
  };

  device.read_holding_registers(0x0102, 3);
  check({0x03, 0x01, 0x02, 0x00, 0x03});
  device.read_input_registers(0x0010, 2);
  check({0x04, 0x00, 0x10, 0x00, 0x02});
  device.read_coils(0x0020, 10);
  check({0x01, 0x00, 0x20, 0x00, 0x0A});
  device.read_discrete_inputs(0x0030, 1);
  check({0x02, 0x00, 0x30, 0x00, 0x01});
  device.write_single_register(0x0040, 0xABCD);
  check({0x06, 0x00, 0x40, 0xAB, 0xCD});
  device.write_single_coil(0x0041, true);
  check({0x05, 0x00, 0x41, 0xFF, 0x00});
  device.write_single_coil(0x0041, false);
  check({0x05, 0x00, 0x41, 0x00, 0x00});
  const uint16_t regs[] = {0x000B, 0x0016};
  device.write_multiple_registers(0x0050, regs);
  check({0x10, 0x00, 0x50, 0x00, 0x02, 0x04, 0x00, 0x0B, 0x00, 0x16});
  const bool coils[] = {true, false, true};
  device.write_multiple_coils(0x0060, coils);
  check({0x0F, 0x00, 0x60, 0x00, 0x03, 0x01, 0x05});
  const uint8_t packed[] = {0x05};
  device.write_multiple_coils(0x0060, PackedBits(packed, 3));  // packed overload, same wire bytes
  check({0x0F, 0x00, 0x60, 0x00, 0x03, 0x01, 0x05});
}

TEST(ModbusTypedSendHelpers, ReadEntitiesDispatchesByTypeAndRejectsInvalid) {
  NoResponseProbeHub hub;
  ModbusClientDevice device(&hub, 0x02);

  device.read_entities(EntityType::HOLDING, 0x0001, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).frame.pdu()[0], 0x03);
  hub.force_send_front();
  hub.timeout_waiting();

  device.read_entities(EntityType::DISCRETE_INPUT, 0x0001, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).frame.pdu()[0], 0x02);
  hub.force_send_front();
  hub.timeout_waiting();

  device.read_entities(EntityType::CUSTOM, 0x0001, 1);  // no read function: logged and not queued
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A rejected read_entities() signals on_not_sent() like every other refused send.
namespace {
class NotSentCountingDevice : public ModbusClientDevice {
 public:
  NotSentCountingDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_++; }
  int not_sent_{0};
};
}  // namespace

TEST(ModbusTypedSendHelpers, InvalidReadEntitiesSignalsNotSent) {
  NoResponseProbeHub hub;
  NotSentCountingDevice device(&hub, 0x02);
  device.read_entities(EntityType::CUSTOM, 0x0001, 1);
  EXPECT_EQ(device.not_sent_, 1);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// Re-sends its own frame from inside on_response() - matching the command mid-completion.
class ResendOnDataDevice : public ModbusClientDevice {
 public:
  ResendOnDataDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->send_pdu(std::vector<uint8_t>(request_pdu.begin(), request_pdu.end()));
  }
  void send_pdu(const std::vector<uint8_t> &pdu) { ModbusClientDevice::send_pdu(pdu); }
};
}  // namespace

// A send from inside on_response() that matches the RECEIVED (completing) entry is absorbed into
// it, so the poll's next cycle serves it - one entry on the queue afterwards, never a fresh twin.
TEST(ModbusClientHubPriority, ResendFromOnResponseAbsorbsIntoCompletingCommand) {
  NoResponseProbeHub hub;
  ResendOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);  // handler re-sends the identical frame mid-completion

  ASSERT_EQ(hub.queued_frames(), 1u);  // absorbed: the cycled resident entry is the only one
  EXPECT_TRUE(hub.queued(0).resident);
}

// An exception-flagged function code is never silently re-sendable, even though the read check
// masks the exception bit: its duplicate takes the drop path like any other non-read.
TEST(ModbusClientHubPriority, ExceptionFlaggedDuplicateDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t weird[] = {0x83, 0x01, 0x00, 0x00, 0x02};  // read-shaped but exception-flagged
  device.send_pdu(weird);
  device.send_pdu(weird);
  hub.sweep_for_test();  // the non-requeueable cap of one bleeds the duplicate off

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);
  EXPECT_EQ(device.not_sent_count_, 1);  // duplicate dropped with a terminal

  // The write-shaped twin (0x86 masks to WRITE_SINGLE_REGISTER) must not take WRITE-class
  // ordering either: exception-flagged codes are excluded from the mutates classification.
  const uint8_t weird_write[] = {0x86, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(weird_write);
  ASSERT_EQ(hub.queued_frames(), 2u);
  EXPECT_EQ(hub.queued(1).priority, CommandPriority::READ);  // not WRITE
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.pdu()[0], 0x83);  // FIFO by age: it did not jump the older entry
}

namespace {
// From inside the sweep's on_not_sent, re-sends the frame that is currently IN FLIGHT.
class ResendInFlightOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendInFlightOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t in_flight[] = {0x03, 0x01, 0x00, 0x00, 0x02};  // == READ_PDU
      this->send_pdu(in_flight);
    }
  }
  int not_sent_count_{0};
};
}  // namespace

// A clear with clear_sent turns the in-flight entry into a detached WAITING_DELETED shell, so a
// sweep handler re-sending that frame queues fresh instead of being absorbed into the dead shell -
// which would have left the request callback-less.
TEST(ModbusClientHubQueue, SweepResendOfInFlightFrameQueuesFreshWhenClearSentDetaches) {
  NoResponseProbeHub hub;
  ResendInFlightOnNotSentDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.force_send_front();  // READ_PDU now in flight
  const uint8_t queued_read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(queued_read);  // a queued frame for the sweep to notify
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.clear_tx_queue_for_address(0x02, /*clear_sent=*/true);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 1);  // the cleared queued frame
  ASSERT_EQ(hub.queued_frames(), 1u);    // the handler's re-send queued fresh...
  EXPECT_EQ(hub.queued(0).pending, 1u);  // ...not absorbed into the dead shell
  EXPECT_TRUE(std::equal(hub.queued(0).frame.pdu().begin(), hub.queued(0).frame.pdu().end(), READ_PDU));
  EXPECT_EQ(hub.waiting_command().state, FrameState::WAITING_DELETED);
  EXPECT_EQ(hub.waiting_command().device, nullptr);  // the shell was detached by the clear
}

// An absorbed extra request also gets its run after an error response - the re-request was
// explicit, so it runs once more whether this attempt succeeded or not.
TEST(ModbusClientHubCallbackCount, AbsorbedRequestRunsAfterErrorResponse) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // in-flight duplicate: absorbed
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);  // error terminal for request 1

  EXPECT_EQ(device.error_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);  // request 2's run still queued
  EXPECT_EQ(hub.queued(0).pending, 1u);
}

// Read-modify-write function codes mutate registers, so they rank as WRITE for transmit ordering.
TEST(ModbusClientHubPriority, ReadModifyWritesRankAsWrites) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t mask_write[] = {0x16, 0x00, 0x10, 0x00, 0xFF, 0x00, 0x01};
  device.send_pdu(read);
  device.send_pdu(mask_write);

  ASSERT_EQ(hub.queued_frames(), 2u);
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->priority, CommandPriority::WRITE);  // 0x16 wins selection over the queued read
  EXPECT_EQ(next->frame.pdu()[0], 0x16);
}
}  // namespace esphome::modbus::testing
