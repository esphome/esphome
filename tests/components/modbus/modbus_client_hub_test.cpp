#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <vector>

#include "common.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::modbus::testing {

namespace {

void ensure_test_app_constructed() {
  static bool app_constructed = false;
  if (!app_constructed) {
    new (&App) Application();
    app_constructed = true;
  }
}

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
  ensure_test_app_constructed();
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
  ensure_test_app_constructed();
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
  ensure_test_app_constructed();
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
  ensure_test_app_constructed();
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
}

// A continuous read: every wire transmission pairs one sent with one terminal, ending on the error.
TEST(ModbusClientHubCallbackCount, ContinuousLifecyclesBalance) {
  ensure_test_app_constructed();
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
  ensure_test_app_constructed();
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  ChainOnSentDevice device(&hub, 0x02);

  device.send_pdu(read_pdu());
  hub.send_next_for_test();  // first frame goes on the wire -> on_sent chains a follow-up

  EXPECT_TRUE(hub.waiting());                           // first frame is in flight
  ASSERT_EQ(hub.queued_frames(), 1u);                   // the follow-up queued behind it, not sent
  EXPECT_EQ(hub.queued(0).frame.data.data()[3], 0x09);  // it is the chained read (start address 0x0009)
}

// KNOWN LIMITATION (documented on ModbusClientDevice): clearing a device's own queue from inside
// on_response() does NOT cancel that command's pending continuous re-queue, because the command was
// moved out of the waiting slot before the callback ran. Pin the current behavior so any change is deliberate.
TEST(ModbusClientHubPriority, ClearDuringDataDoesNotCancelContinuousRequeue) {
  NoResponseProbeHub hub;
  ClearOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_front();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);

  EXPECT_EQ(hub.queued_frames(), 1u);  // re-queued despite the mid-callback clear (documented limitation)
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

}  // namespace esphome::modbus::testing
