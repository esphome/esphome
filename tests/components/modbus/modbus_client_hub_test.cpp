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

// Exposes the protected tx queue and waiting-for-response slot so tests can drive the
// no-response path without a UART: force_send_front() mimics send_next_frame_() moving the
// front frame in flight, timeout_waiting() mimics the loop() no-response timeout handling.
class NoResponseProbeHub : public ModbusClientHub {
 public:
  size_t queued_frames() const { return this->tx_buffer_.size(); }
  const ModbusDeviceCommand &front() const { return this->tx_buffer_.front(); }
  const ModbusDeviceCommand &queued(size_t i) const { return this->tx_buffer_[i]; }
  bool waiting() const { return this->waiting_for_response_.has_value(); }
  const ModbusDeviceCommand &waiting_command() const {
    EXPECT_TRUE(this->waiting_for_response_.has_value());
    return *this->waiting_for_response_;  // NOLINT(bugprone-unchecked-optional-access)
  }

  void send_next_for_test() { this->send_next_frame_(); }
  void force_send_front() {
    this->waiting_for_response_ = std::move(this->tx_buffer_.front());
    this->tx_buffer_.pop_front();
  }
  // Drives the real unexpected-frame branch in process_modbus_server_frame().
  void receive_frame_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->process_modbus_server_frame(address, pdu);
  }
  void timeout_waiting() {
    if (this->waiting_for_response_.has_value())
      this->notify_no_response_(*this->waiting_for_response_);
    this->waiting_for_response_.reset();
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
  const ModbusDeviceCommand &requeued = hub.front();
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

// An unexpected frame interrupts the transaction: the retry is re-queued immediately, but the
// waiting entry survives as an interrupted shell (device detached) that keeps tx blocked until the
// send-wait timeout clears it - without a second no-response callback or a duplicate requeue.
TEST(ModbusClientHubNoResponse, RetryBehindInterruptedShell) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();

  // A frame from the wrong address (0x07, expected 0x02) hits the unexpected-frame branch.
  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);

  EXPECT_EQ(device.no_response_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);  // exactly one requeue...
  EXPECT_EQ(hub.front().device, &device);
  ASSERT_TRUE(hub.waiting());  // ...while the shell stays in the waiting slot
  EXPECT_TRUE(hub.waiting_command().interrupted);
  EXPECT_EQ(hub.waiting_command().device, nullptr);

  // The send-wait timeout clears the shell without a second callback or another requeue.
  hub.timeout_waiting();
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(hub.queued_frames(), 1u);
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
  EXPECT_EQ(hub.queued(0).frame.data.data()[1], 0x06);  // the write moved to the front
  EXPECT_EQ(hub.queued(1).frame.data.data()[2], 0x01);  // reads stay FIFO behind it
  EXPECT_EQ(hub.queued(2).frame.data.data()[2], 0x02);
}

// Re-requesting a queued frame promotes the existing entry instead of queueing a duplicate.
TEST(ModbusClientHubPriority, DuplicateQueuedFramePromotedNotDuplicated) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.send_pdu(read_pdu());
  device.send_pdu(read_pdu());

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_AGAIN);
}

// Re-requesting the frame currently in flight promotes the waiting entry; after a no-response
// timeout it re-queues exactly once (demoted to READ_ONCE) even though the device declines a retry,
// and a second timeout does not re-queue again.
TEST(ModbusClientHubPriority, InFlightDuplicateRunsOnceMore) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // duplicate of the in-flight frame

  EXPECT_EQ(hub.queued_frames(), 0u);  // not queued twice
  EXPECT_EQ(hub.waiting_command().priority, CommandPriority::READ_AGAIN);

  hub.timeout_waiting();
  ASSERT_EQ(hub.queued_frames(), 1u);  // re-queued despite retry=false: it was explicitly re-requested
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_ONCE);  // and demoted

  hub.force_send_front();
  hub.timeout_waiting();
  EXPECT_EQ(hub.queued_frames(), 0u);  // demoted command does not re-queue again
}

// A READ_AGAIN entry carries an extra request the dedup absorbed. When it times out and the device asks
// to retry, that attempt is not a resolution, so it stays READ_AGAIN (both requests remain pending) rather
// than being demoted - which would drop one request and leave that caller without a resolution.
TEST(ModbusClientHubPriority, ReadAgainKeepsExtraRequestWhenDeviceRetries) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // duplicate of the in-flight frame -> promoted to READ_AGAIN
  ASSERT_EQ(hub.waiting_command().priority, CommandPriority::READ_AGAIN);

  hub.timeout_waiting();  // no response; the device requests a retry

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_AGAIN);  // preserved, not demoted
}

// A continuous read re-queues itself (at the lowest priority) after each successful response,
// but not after an exception response.
TEST(ModbusClientHubPriority, ContinuousReadRequeuesOnSuccessOnly) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);
  hub.force_send_front();

  // A matching successful response completes the command and re-queues it.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);

  // An exception response completes it without re-queueing.
  hub.force_send_front();
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A continuous read that gets no response and is retried stays continuous: an explicit retry of a
// continuous poll is assumed to still want continuous polling (it does not drop to a one-shot READ_ONCE).
TEST(ModbusClientHubPriority, RetriedContinuousReadStaysContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);
  hub.force_send_front();

  hub.timeout_waiting();  // no response -> device requests retry

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);  // retry stays continuous
}

// A duplicate send never promotes a continuous entry: promotion to READ_AGAIN would destroy the
// continuous flag and polling would silently stop after two more runs. The duplicate request is
// served by the poll's next response instead.
TEST(ModbusClientHubPriority, DuplicateSendKeepsContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  device.read_holding_registers(0x100, 2);  // queued duplicate: absorbed, not promoted
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);

  hub.force_send_front();
  device.read_holding_registers(0x100, 2);  // in-flight duplicate: absorbed, not promoted
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(hub.waiting_command().priority, CommandPriority::READ_CONTINUOUS);

  // The poll survives the duplicates: a successful response still re-queues it as continuous.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);
}

// Requesting continuous polling for a frame that is already queued as a one-shot turns that entry
// into the continuous poll instead of leaving a promotion that never polls.
TEST(ModbusClientHubPriority, ContinuousRequestUpgradesQueuedDuplicate) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2);
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.queued(0).priority, CommandPriority::READ_ONCE);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);

  // And it behaves as a poll from here: success re-queues it.
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_CONTINUOUS);
}

// continuous is ignored for writes: the frame still sends at WRITE priority, once.
TEST(ModbusClientHubPriority, ContinuousIgnoredForWrites) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);
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

// A write is never requeueable: a duplicate of a queued write is dropped (reported not_sent), and the
// queued write keeps WRITE priority rather than being promoted to READ_AGAIN and sent an extra time.
TEST(ModbusClientHubPriority, DuplicateQueuedWriteDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(write_pdu);  // duplicate write

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);  // not promoted to READ_AGAIN
  EXPECT_EQ(device.not_sent_count_, 1);                       // the duplicate was dropped
}

// Requeueability is an allow-list of the standard reads: a custom function code's idempotency is
// unknown, so its duplicate takes the same safe drop path as a write instead of a silent re-send.
TEST(ModbusClientHubPriority, DuplicateCustomFunctionCodeDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t custom_pdu[] = {0x41, 0x01, 0x02};  // user-defined function code
  device.send_pdu(custom_pdu);
  device.send_pdu(custom_pdu);  // duplicate custom command

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_ONCE);  // not promoted
  EXPECT_EQ(device.not_sent_count_, 1);                           // the duplicate was dropped
}

// An anonymous duplicate (no device - the YAML-lambda path) is always dropped, never promoted:
// with no callback there is no lifecycle to absorb into and no owner for a READ_AGAIN re-run.
TEST(ModbusClientHubPriority, AnonymousDuplicateDroppedNotPromoted) {
  NoResponseProbeHub hub;

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  hub.send_pdu(0x02, read);
  hub.send_pdu(0x02, read);  // anonymous duplicate: dropped

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_ONCE);  // never promoted for a null owner
}

// A re-queued READ_AGAIN entry outranks fresh READ_ONCE reads already in the queue: a device-
// requested retry keeps the priority (both absorbed requests still pending), so the re-queue
// inserts ahead of reads that arrived while it was in flight.
TEST(ModbusClientHubPriority, ReadAgainOutranksFreshReads) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.send_pdu(read_pdu());
  hub.force_send_front();       // the frame that will come back as READ_AGAIN
  device.send_pdu(read_pdu());  // in-flight duplicate: promotes the waiting entry
  const uint8_t fresh_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t fresh_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  device.send_pdu(fresh_a);
  device.send_pdu(fresh_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.timeout_waiting();  // device retries; READ_AGAIN is preserved and must outrank the fresh reads

  ASSERT_EQ(hub.queued_frames(), 3u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::READ_AGAIN);
  EXPECT_TRUE(std::equal(hub.queued(0).frame.pdu().begin(), hub.queued(0).frame.pdu().end(), READ_PDU));
  EXPECT_EQ(hub.queued(1).frame.pdu()[2], 0x10);  // fresh reads keep FIFO order behind it
  EXPECT_EQ(hub.queued(2).frame.pdu()[2], 0x20);
}

// A write that is retried after a no-response keeps WRITE priority (not demoted to READ_ONCE), so it
// stays ahead of reads and a later duplicate still dedupes immediately instead of promoting it.
TEST(ModbusClientHubPriority, RetriedWriteKeepsWritePriorityAndStaysNonRequeueable) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  hub.force_send_front();
  hub.timeout_waiting();  // no response -> device requests retry -> re-queued

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);  // retry preserves WRITE, not READ_ONCE

  device.send_pdu(write_pdu);                                 // duplicate of the retried write
  ASSERT_EQ(hub.queued_frames(), 1u);                         // still not queued twice...
  EXPECT_EQ(hub.queued(0).priority, CommandPriority::WRITE);  // ...and still not promoted to READ_AGAIN
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
  EXPECT_EQ(device.not_sent_count_, 1);  // the third request is refused immediately
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

  // Unpromotable duplicate: two identical writes -> the second is a not_sent terminal, never sent.
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(write_pdu);
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

// A retry re-queue that finds the buffer full is refused like any other send: the device gets
// on_not_sent() carrying the request PDU (the previously uncovered requeue_waiting_frame_ branch).
TEST(ModbusClientHubCallbackCount, FullQueueRetryRefusalDeliversNotSentWithPdu) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);
  device.retries_ = 1;
  SentCountingDevice filler(&hub, 0x05);

  device.send_pdu(read_pdu());
  hub.force_send_front();  // in flight
  // Fill the queue with distinct frames.
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  hub.timeout_waiting();  // retry requested, but the re-queue is refused: not_sent terminal instead

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 1);
  EXPECT_EQ(device.last_not_sent_pdu_, std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU)));
  EXPECT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);
}

// The completed-re-queue path's full-buffer refusal: a READ_AGAIN entry whose post-completion
// re-run cannot queue delivers the absorbed request's on_not_sent() with the request PDU.
TEST(ModbusClientHubCallbackCount, FullQueueCompletedRequeueRefusalDeliversNotSent) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);
  SentCountingDevice filler(&hub, 0x05);

  device.send_pdu(read_pdu());
  hub.force_send_front();
  device.send_pdu(read_pdu());  // in-flight duplicate: promotes to READ_AGAIN
  // Fill the queue with distinct frames.
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);  // completes request 1; the re-run is refused

  EXPECT_EQ(device.data_count_, 1);      // first absorbed request resolved by the response
  EXPECT_EQ(device.not_sent_count_, 1);  // second absorbed request resolved by the refusal
  EXPECT_EQ(device.terminals(), 2);
  EXPECT_EQ(device.last_not_sent_pdu_, std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU)));
  EXPECT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);
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

  ASSERT_EQ(hub.queued_frames(), 1u);  // only the other-address frame remains
  EXPECT_EQ(hub.front().frame.address(), 0x03);
  EXPECT_EQ(controller_like.not_sent_count_, 1);
  EXPECT_EQ(bystander_same.not_sent_count_, 1);
  EXPECT_EQ(bystander_other.not_sent_count_, 0);
  // each owner saw its own request PDU
  EXPECT_EQ(bystander_same.last_not_sent_pdu_, std::vector<uint8_t>(std::begin(read_b), std::end(read_b)));
}

// A swept READ_AGAIN frame stands for exactly two accepted requests (the original and the one
// absorbed by promotion; a third duplicate is refused rather than absorbed), so the sweep resolves
// it with two on_not_sent() deliveries - the books balance for owners counting outstanding requests.
TEST(ModbusClientHubQueue, ClearAddressSweptReadAgainDeliversBothNotSent) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  device.send_pdu(read);
  device.send_pdu(read);  // duplicate: promotes the queued entry to READ_AGAIN
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.front().priority, CommandPriority::READ_AGAIN);

  hub.clear_tx_queue_for_address(0x02, false);

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

// A handler that re-sends to the same address from inside on_not_sent() neither corrupts the sweep nor
// loops it: only initially-marked frames are swept, so the re-queued frame stays queued.
TEST(ModbusClientHubQueue, ClearAddressReentrantResendSurvives) {
  NoResponseProbeHub hub;
  ResendOnNotSentDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.clear_tx_queue_for_address(0x02, false);

  // The original frame resolved via on_not_sent; the re-send from inside that callback remains queued.
  EXPECT_EQ(device.not_sent_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.front().frame.address(), 0x02);
}

// The hard case for the sweep: the notified handler re-queues a WRITE to the cleared address, which
// insert_by_priority_() places AHEAD of a remaining other-address read. The re-queued frame is unmarked,
// so the sweep must neither drop it nor notify its owner a second time - and the bystander's frame at the
// other address survives untouched.
TEST(ModbusClientHubQueue, ClearAddressReentrantResendNotSweptDespitePriorityInsert) {
  NoResponseProbeHub hub;
  ResendOnNotSentDevice resender(&hub, 0x02);
  SentCountingDevice bystander_other(&hub, 0x03);

  const uint8_t read_victim[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_other[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  resender.send_pdu(read_victim);
  bystander_other.send_pdu(read_other);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02, false);

  EXPECT_EQ(resender.not_sent_count_, 1);  // notified once, never re-notified for the re-send
  ASSERT_EQ(hub.queued_frames(), 2u);      // the re-queued write AND the other-address read survive
  // the write re-queued from inside the callback outranks the read and sits at the front
  EXPECT_EQ(hub.front().frame.address(), 0x02);
  EXPECT_EQ(hub.front().frame.pdu()[0], 0x06);
  EXPECT_EQ(hub.queued(1).frame.address(), 0x03);
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

// A handler that retries from every on_not_sent() against a FULL queue must not recurse: the first
// refusal notifies once, the nested refusal is dropped without a callback (the documented guard).
TEST(ModbusClientHubQueue, FullQueueRetryFromNotSentDoesNotRecurse) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  AlwaysRetryDevice retrier(&hub, 0x02);

  // Fill the queue with distinct frames.
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  retrier.send_pdu(read);  // refused (full) -> on_not_sent -> retry -> refused under the guard, silently

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

// The refusal recursion guard is per-device: a refusal that lands on a DIFFERENT device while one
// device's notification is on the stack must still deliver - that device did not cause the recursion
// and would otherwise silently lose its terminal callback.
TEST(ModbusClientHubQueue, RefusalForOtherDeviceDeliversDuringNotification) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  SendOtherOnNotSentDevice first(&hub, 0x02);
  SentCountingDevice second(&hub, 0x03);
  first.other_ = &second;

  // Fill the queue with distinct frames.
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  first.send_pdu(read);  // refused -> first.on_not_sent -> second's send refused -> second notified

  EXPECT_EQ(first.not_sent_count_, 1);
  EXPECT_EQ(second.not_sent_count_, 1);
}

// Two devices whose handlers each trigger the other's send cannot recurse without bound: each device
// can be on the notification stack at most once, so the cycle dies as soon as it returns to a device
// whose own on_not_sent() is still running.
TEST(ModbusClientHubQueue, TwoDeviceRefusalCycleTerminates) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  SendOtherOnNotSentDevice first(&hub, 0x02);
  SendOtherOnNotSentDevice second(&hub, 0x03);
  first.other_ = &second;
  second.other_ = &first;

  // Fill the queue with distinct frames.
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.send_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  first.send_pdu(read);  // refuse -> first -> second refused -> second -> first suppressed -> unwind

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

// The documented cost of the per-device guard: a clear issued from inside your own on_not_sent()
// resolves your remaining frames silently (like clear_tx_queue_for_device() - you cleared them, you
// know), while other owners sharing the address are still notified.
TEST(ModbusClientHubQueue, SelfClearFromNotSentSilentForClearerNotifiesOthers) {
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

  clearer.send_pdu(std::span<const uint8_t>{});  // refused (empty) -> the handler clears the shared address

  EXPECT_EQ(clearer.not_sent_count_, 1);    // only the refusal; the two swept frames resolve silently
  EXPECT_EQ(bystander.not_sent_count_, 1);  // the bystander's swept frame is still notified
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// The guard must not over-suppress: a sweep started from inside on_not_sent() still delivers its
// victims' notifications (only nested refusals are silenced).
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

  EXPECT_EQ(clearer.not_sent_count_, 1);
  EXPECT_EQ(victim.not_sent_count_, 1);  // delivered despite arriving from a nested sweep
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

// Reacts to a transmit failure by sending a WRITE, which insert_by_priority_() puts at the queue front.
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

// From on_not_sent (delivered by the sweep), re-sends a frame identical to ANOTHER still-marked queued
// frame; the dedup must not promote the doomed entry.
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

// A transmit failure must resolve with the failed frame OUT of the queue before its on_not_sent runs: a
// handler that reacts by sending a write (priority-inserted at the FRONT) must not have that write
// discarded by the pop that follows - the failed frame is popped first, the new write survives.
TEST(ModbusClientHubQueue, TransmitFailurePopsBeforeNotify) {
  FlakyBlockHub hub;
  WriteOnNotSentDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.send_next_for_test();  // tx_blocked gate passes, send_frame_ refuses -> failure path

  EXPECT_EQ(device.not_sent_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);           // the handler's write survives...
  EXPECT_EQ(hub.front().frame.pdu()[0], 0x06);  // ...and it is the write, not the failed read
}

// A send during a sweep that matches a still-marked (doomed) frame must queue fresh, not promote the
// marked entry - promotion would absorb the new request into a frame the sweep then drops.
TEST(ModbusClientHubQueue, SweepDedupSkipsMarkedFrames) {
  NoResponseProbeHub hub;
  ResendSecondFrameDevice device(&hub, 0x02);

  const uint8_t r1[] = {0x03, 0x00, 0x21, 0x00, 0x01};
  const uint8_t r2[] = {0x03, 0x00, 0x22, 0x00, 0x01};
  device.send_pdu(r1);
  device.send_pdu(r2);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02, false);
  // r1's notification re-sent a frame identical to the still-marked r2. Without the dedup skip it is
  // promoted onto r2 and then swept away with it; with the skip it queues fresh and survives.

  EXPECT_EQ(device.not_sent_count_, 2);  // r1 and r2 both resolved
  ASSERT_EQ(hub.queued_frames(), 1u);    // the re-send survives
  EXPECT_EQ(hub.front().frame.pdu()[2], 0x22);
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

// KNOWN LIMITATION (documented on ModbusClientDevice): clearing a device's own queue from inside
// on_response() does NOT cancel that command's pending continuous re-queue, because the command was
// moved out of the waiting slot before the callback ran. Pin the current behavior so any change is deliberate.
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
    auto pdu = hub.front().frame.pdu();
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
  EXPECT_EQ(hub.front().frame.pdu()[0], 0x03);
  hub.force_send_front();
  hub.timeout_waiting();

  device.read_entities(EntityType::DISCRETE_INPUT, 0x0001, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.front().frame.pdu()[0], 0x02);
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

}  // namespace esphome::modbus::testing
