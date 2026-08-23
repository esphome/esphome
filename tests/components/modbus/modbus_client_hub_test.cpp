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

// Exposes the frame state machine so tests can drive it without a UART (force_send_next(),
// timeout_waiting(), sweep_for_test() stand in for the loop() transmit/watchdog/sweep steps).
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
  // A never-null placeholder to return when a lookup fails, so a tripped EXPECT/ADD_FAILURE reports
  // the assertion instead of dereferencing null / an empty deque and segfaulting the whole suite.
  static const ModbusDeviceCommand &dummy_command() {
    static const uint8_t DUMMY_PDU[1] = {0x00};
    static ModbusDeviceCommand cmd(nullptr, 0, std::span<const uint8_t>(DUMMY_PDU, 1));
    return cmd;
  }
  const ModbusDeviceCommand &queued(size_t i) const {
    for (const auto &cmd : this->tx_buffer_) {
      if (cmd.state == FrameState::READY && i-- == 0)
        return cmd;
    }
    ADD_FAILURE() << "no READY entry at that index";
    return dummy_command();
  }
  size_t entries() const { return this->tx_buffer_.size(); }
  const ModbusDeviceCommand *next_ready() { return this->select_next_ready_(); }
  bool waiting() const { return this->waiting_for_response_; }
  const ModbusDeviceCommand &waiting_command() {
    ModbusDeviceCommand *cmd = this->find_waiting_();
    EXPECT_NE(cmd, nullptr);
    return cmd != nullptr ? *cmd : dummy_command();
  }

  void sweep_for_test() { this->sweep_(); }
  void send_next_for_test() {
    this->send_next_frame_();
    this->sweep_();  // a transmit failure's on_not_sent() is delivered by the loop's sweep
  }
  void force_send_next() {
    ModbusDeviceCommand *cmd = this->select_next_ready_();
    ASSERT_NE(cmd, nullptr) << "no READY entry to send";
    cmd->state = FrameState::WAITING;
    this->waiting_for_response_ = true;
  }
  // Drives the real response/interruption branches, followed by the loop's sweep.
  void receive_frame_for_test(uint8_t address, std::span<const uint8_t> pdu) {
    this->process_modbus_server_frame(address, pdu);
    this->sweep_();
  }
  void timeout_waiting() {
    this->sweep_();  // deliver anything already owed (e.g. an interruption's on_no_response)
    this->expire_waiting_();
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

  device.queue_pdu(read_pdu());
  ASSERT_EQ(hub.queued_frames(), 1u);
  hub.force_send_next();
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

  device.queue_pdu(read_pdu());
  hub.force_send_next();

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
    device.queue_pdu(read_pdu());
    hub.force_send_next();
    // device destructor clears its queue entries, including the waiting frame's device pointer
  }
  ASSERT_TRUE(hub.waiting());
  EXPECT_EQ(hub.waiting_command().device, nullptr);

  hub.timeout_waiting();

  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// An unexpected frame interrupts the transaction: the entry becomes an INTERRUPTED shell that
// ignores this transaction and blocks tx until the send-wait timeout, where it gets its single
// on_no_response() - a granted retry is requeued there, like any other timeout.
TEST(ModbusClientHubNoResponse, RetryBehindInterruptedShell) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.queue_pdu(read_pdu());
  hub.force_send_next();

  // A frame from the wrong address (0x07, expected 0x02) hits the unexpected-frame branch.
  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);
  hub.sweep_for_test();

  EXPECT_EQ(device.no_response_count_, 0);  // not notified early: it waits out the timeout
  ASSERT_TRUE(hub.waiting());               // and keeps blocking the bus
  EXPECT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED);
  EXPECT_EQ(hub.waiting_command().pending, 1u);

  // The send-wait timeout delivers on_no_response and requeues the granted retry.
  hub.timeout_waiting();
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(device.no_response_count_, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).device, &device);
}

// The declined-retry interrupted shell blocks until the send-wait timeout, then gets its single
// on_no_response() there and retires with nothing left to send.
TEST(ModbusClientHubNoResponse, InterruptedShellDeclinedRetryRetiresOnRelease) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.queue_pdu(read_pdu());
  hub.force_send_next();

  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);  // wrong address: interrupts the transaction
  hub.sweep_for_test();

  EXPECT_EQ(device.no_response_count_, 0);  // not notified early
  ASSERT_TRUE(hub.waiting());               // the shell still blocks the wire
  EXPECT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED);
  EXPECT_EQ(hub.waiting_command().pending, 1u);

  hub.timeout_waiting();  // on_no_response (declined), then the shell retires

  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(device.no_response_count_, 1);
}

// A callback that detaches the device (clear_tx_queue_for_device()) wins over its own retry request:
// no orphaned frame with a null device is re-queued.
TEST(ModbusClientHubNoResponse, MidCallbackClearCancelsRetry) {
  NoResponseProbeHub hub;
  ClearingRetryDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
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
  device.queue_pdu(read_a);
  device.queue_pdu(read_b);
  device.queue_pdu(write_pdu);

  ASSERT_EQ(hub.queued_frames(), 3u);
  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[0], 0x06);  // the write transmits first
  hub.timeout_waiting();
  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[1], 0x01);  // reads follow in FIFO order
  hub.timeout_waiting();
  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[1], 0x02);
}

// Re-requesting a queued frame is absorbed into the existing entry instead of queueing a duplicate.
TEST(ModbusClientHubPriority, DuplicateQueuedFrameAbsorbedNotDuplicated) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.queue_pdu(read_pdu());
  device.queue_pdu(read_pdu());

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 2u);  // one entry standing for two accepted requests
}

// Re-requesting the frame currently waiting is absorbed into the waiting entry; after a
// no-response timeout the absorbed request still gets its run even though the device declines a
// retry, and a second timeout does not run it again.
TEST(ModbusClientHubPriority, InFlightDuplicateRunsOnceMore) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  device.queue_pdu(read_pdu());  // duplicate of the waiting frame

  EXPECT_EQ(hub.queued_frames(), 0u);  // not queued twice
  EXPECT_EQ(hub.waiting_command().pending, 2u);

  hub.timeout_waiting();
  ASSERT_EQ(hub.queued_frames(), 1u);  // the timeout resolved one request; the absorbed one runs
  EXPECT_EQ(hub.queued(0).pending, 1u);

  hub.force_send_next();
  hub.timeout_waiting();
  EXPECT_EQ(hub.queued_frames(), 0u);  // the last request resolved; nothing left to run
}

// An entry with an absorbed extra request that times out while the device asks to retry: the
// retry is not a resolution, so BOTH requests remain pending rather than one being dropped -
// which would leave that caller without a resolution.
TEST(ModbusClientHubPriority, AbsorbedRequestSurvivesDeviceRetry) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  device.queue_pdu(read_pdu());  // duplicate of the waiting frame -> absorbed
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
  EXPECT_TRUE(hub.queued(0).options.continuous);
  hub.force_send_next();

  // A matching successful response cycles the continuous entry back to READY.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).options.continuous);

  // An exception response ends the poll.
  hub.force_send_next();
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A continuous read that gets no response and is retried stays continuous: an explicit retry of a
// continuous poll is assumed to still want continuous polling (the entry stays continuous).
TEST(ModbusClientHubPriority, RetriedContinuousReadStaysContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_TRUE(hub.queued(0).options.continuous);
  hub.force_send_next();

  hub.timeout_waiting();  // no response -> device requests retry

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).options.continuous);  // the retried poll stays continuous
}

// A one-shot duplicate downgrades a continuous poll to a one-shot (the mirror of a continuous
// duplicate upgrading a one-shot): the entry runs one more cycle to serve the request, then stops.
TEST(ModbusClientHubPriority, DuplicateSendDowngradesContinuous) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_TRUE(hub.queued(0).options.continuous);

  device.read_holding_registers(0x100, 2);  // one-shot duplicate downgrades the poll
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_FALSE(hub.queued(0).options.continuous);
  EXPECT_EQ(hub.queued(0).pending, 1u);

  // It runs one more cycle to serve the request, then stops - not re-queued as a poll.
  hub.force_send_next();
  EXPECT_FALSE(hub.waiting_command().options.continuous);
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(hub.entries(), 0u);
}

namespace {
// Re-sends its frame once as a one-shot from inside on_error(), to exercise the downgrade branch
// when the poll it duplicates has already reached a terminal (pending drained to 0).
class ResendOnErrorDevice : public ModbusClientDevice {
 public:
  ResendOnErrorDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) override {
    this->error_count_++;
    if (this->resend_) {
      this->resend_ = false;
      this->read_holding_registers(0x100, 2);  // one-shot re-send from inside the failure callback
    }
  }
  int error_count_{0};
  bool resend_{true};
};
}  // namespace

// A one-shot re-send issued from inside a continuous poll's failure callback must still run. The
// poll's exception terminal has already drained pending to 0, so the re-send absorbs into that entry
// via the downgrade branch - which must restore the debt, or the sweep erases the entry with the
// request never sent and no callback delivered.
TEST(ModbusClientHubPriority, DowngradeAfterTerminalKeepsRequestAlive) {
  NoResponseProbeHub hub;
  ResendOnErrorDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_TRUE(hub.queued(0).options.continuous);

  hub.force_send_next();
  const uint8_t exception_response[] = {0x83, 0x02};
  hub.receive_frame_for_test(0x02, exception_response);  // exception ends the poll; on_error re-sends

  EXPECT_EQ(device.error_count_, 1);               // one terminal delivered so far
  ASSERT_EQ(hub.queued_frames(), 1u);              // the re-send survived the sweep instead of being erased
  EXPECT_FALSE(hub.queued(0).options.continuous);  // downgraded to a one-shot
  EXPECT_EQ(hub.queued(0).pending, 1u);            // debt restored so the request runs

  // And it runs to its own terminal - a good response this time - then the entry is gone.
  hub.force_send_next();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(hub.entries(), 0u);
}

// Requesting continuous polling for a frame that is already queued as a one-shot turns that entry
// into the continuous poll instead of leaving a promotion that never polls.
TEST(ModbusClientHubPriority, ContinuousRequestUpgradesQueuedDuplicate) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  device.read_holding_registers(0x100, 2);
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_FALSE(hub.queued(0).options.continuous);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).options.continuous);

  // And it behaves as a poll from here: success cycles it back to READY.
  hub.force_send_next();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_TRUE(hub.queued(0).options.continuous);
}

// The transmit order is one key with three levels: writes, then one-shot reads, then continuous
// polls - a poll only gets the bus when nothing else wants it.
TEST(ModbusClientHubPriority, WritesThenOneShotReadsThenContinuousPolls) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  // Queued oldest-first in the opposite order to the one they must transmit in, so age cannot be
  // what produces the expected sequence.
  device.read_holding_registers(0x100, 2, {.continuous = true});
  const uint8_t one_shot[] = {0x03, 0x02, 0x00, 0x00, 0x01};
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.queue_pdu(one_shot);
  device.queue_pdu(write_pdu);
  ASSERT_EQ(hub.queued_frames(), 3u);
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::CONTINUOUS);
  EXPECT_EQ(hub.queued(1).priority(), CommandPriority::READ);
  EXPECT_EQ(hub.queued(2).priority(), CommandPriority::WRITE);

  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[0], 0x06);  // the write goes first
  hub.timeout_waiting();
  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[1], 0x02);  // then the one-shot read
  hub.timeout_waiting();
  hub.force_send_next();
  EXPECT_TRUE(hub.waiting_command().options.continuous);  // and the poll takes what is left
}

// continuous is ignored for writes: the frame still sends at WRITE priority, once.
TEST(ModbusClientHubPriority, ContinuousIgnoredForWrites) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.queue_pdu(write_pdu, {.continuous = true});
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::WRITE);
  EXPECT_FALSE(hub.queued(0).options.continuous);
}

// A queued continuous poll does not count against immediate-send readiness: it ranks below every
// one-shot, so a new one-shot goes out ahead of it. A queued one-shot does count.
TEST(ModbusClientHubPriority, ContinuousPollDoesNotBlockImmediateSend) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  EXPECT_TRUE(hub.tx_buffer_empty());  // nothing queued
  device.read_holding_registers(0x100, 2, {.continuous = true});
  ASSERT_TRUE(hub.queued(0).options.continuous);
  EXPECT_TRUE(hub.tx_buffer_empty());  // a READY continuous poll still leaves room to send now

  device.read_holding_registers(0x200, 2);  // a one-shot does count
  EXPECT_FALSE(hub.tx_buffer_empty());
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

// A write is never requeueable, so its entry can serve exactly one request: a duplicate of a
// queued write is refused at the door rather than earning the write an extra transmission.
TEST(ModbusClientHubPriority, DuplicateQueuedWriteRefused) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  EXPECT_TRUE(device.queue_pdu(write_pdu));
  EXPECT_FALSE(device.queue_pdu(write_pdu));  // duplicate write: refused
  hub.sweep_for_test();

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::WRITE);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // a write's cap
  EXPECT_EQ(device.not_sent_count_, 0);  // refusals are returned, never delivered
}

// Requeueability is an allow-list of the standard reads: a custom function code's idempotency is
// unknown, so its duplicate is refused like a write's instead of earning a silent re-send.
TEST(ModbusClientHubPriority, DuplicateCustomFunctionCodeRefused) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t custom_pdu[] = {0x41, 0x01, 0x02};  // user-defined function code
  EXPECT_TRUE(device.queue_pdu(custom_pdu));
  EXPECT_FALSE(device.queue_pdu(custom_pdu));  // duplicate custom command: refused
  hub.sweep_for_test();

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // the non-requeueable cap of one run
  EXPECT_EQ(device.not_sent_count_, 0);
}

// An anonymous duplicate (no device - the YAML-lambda path) is always dropped, never promoted:
// with no callback there is no lifecycle to absorb into and no owner to route a re-run to.
TEST(ModbusClientHubPriority, AnonymousDuplicateDroppedNotPromoted) {
  NoResponseProbeHub hub;

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  hub.queue_pdu(0x02, read);
  hub.queue_pdu(0x02, read);  // anonymous duplicate: dropped

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);  // never absorbed for a null owner
}

// A retried entry is re-stamped to the queue tail: reads that arrived while it was waiting get
// their turn before the retry, so a frame that keeps timing out cannot starve the rest of the bus.
TEST(ModbusClientHubPriority, RetriedReadGoesBehindFreshReads) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/true);

  device.queue_pdu(read_pdu());
  hub.force_send_next();         // the frame that will time out and retry
  device.queue_pdu(read_pdu());  // waiting duplicate: absorbed into the waiting entry
  const uint8_t fresh_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t fresh_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  device.queue_pdu(fresh_a);
  device.queue_pdu(fresh_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.timeout_waiting();  // device retries; the entry returns to READY behind the fresh reads

  ASSERT_EQ(hub.queued_frames(), 3u);
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.pdu()[2], 0x10);  // fresh reads keep FIFO order ahead of the retry
  hub.force_send_next();
  hub.timeout_waiting();
  hub.force_send_next();
  EXPECT_EQ(hub.waiting_command().frame.pdu()[2], 0x20);
  hub.timeout_waiting();
  hub.force_send_next();  // the retry gets its turn last, both requests still on the entry
  EXPECT_TRUE(std::equal(hub.waiting_command().frame.pdu().begin(), hub.waiting_command().frame.pdu().end(), READ_PDU));
  EXPECT_EQ(hub.waiting_command().pending, 2u);
}

// An absorbed duplicate does not move the entry back in line: seq belongs to the entry, and only
// re-entering the line (retry, resolved request, continuous cycle) re-stamps it.
TEST(ModbusClientHubPriority, AbsorbedDuplicateKeepsPlaceInLine) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t read_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  device.queue_pdu(read_a);
  device.queue_pdu(read_b);
  device.queue_pdu(read_a);  // duplicate of the older entry: absorbed, place unchanged
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
  device.queue_pdu(write_pdu);
  hub.force_send_next();
  hub.timeout_waiting();  // no response -> device requests retry -> back to READY

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::WRITE);  // retry preserves the WRITE class

  device.queue_pdu(write_pdu);         // duplicate of the retried write
  ASSERT_EQ(hub.queued_frames(), 1u);  // still not queued twice...
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::WRITE);
  hub.sweep_for_test();
  EXPECT_EQ(hub.queued(0).pending, 1u);  // ...the duplicate was refused at the door (write cap is 1)
}

namespace {
// A hub that is never free to transmit.
class AlwaysBlockedHub : public NoResponseProbeHub {
 public:
  bool tx_blocked() override { return true; }
};
}  // namespace

// Transmitting cannot fail, so a hub that is busy simply does not transmit: the frame keeps its
// place in the queue and goes out on a later loop, with no callback and no lifecycle change. (The
// caller owns the tx_blocked() check; send_frame_() has no gate of its own to refuse at.)
TEST(ModbusClientHubSent, BlockedHubDefersInsteadOfFailing) {
  AlwaysBlockedHub hub;
  SentCountingDevice device(&hub, 0x02);

  EXPECT_TRUE(device.queue_pdu(read_pdu()));
  hub.send_next_for_test();

  EXPECT_EQ(device.sent_count_, 0);
  EXPECT_EQ(device.not_sent_count_, 0);  // nothing failed - it has not been attempted
  ASSERT_EQ(hub.queued_frames(), 1u);    // still queued, still owed exactly one terminal
  EXPECT_EQ(hub.queued(0).pending, 1u);
  EXPECT_FALSE(hub.waiting());
}

// on_sent() fires when the frame goes onto the wire, not when it is queued.
TEST(ModbusClientHubSent, FiresOnWireNotOnQueue) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();  // frame timing derives from the baud rate
  SentCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  EXPECT_EQ(device.sent_count_, 0);  // queued only - nothing sent yet

  hub.send_next_for_test();
  EXPECT_EQ(device.sent_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 0);
  // The callback identifies which command transmitted: it carries the request PDU.
  EXPECT_EQ(device.last_sent_pdu_, (std::vector<uint8_t>(READ_PDU, READ_PDU + sizeof(READ_PDU))));
  EXPECT_TRUE(hub.waiting());
}

namespace {
// Records on_sent / on_response / on_no_response so a broadcast's fire-and-forget completion
// (on_sent, and no terminal) can be asserted.
class BroadcastProbeDevice : public ModbusClientDevice {
 public:
  BroadcastProbeDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_sent(std::span<const uint8_t> request_pdu) override { this->sent_count_++; }
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->response_count_++;
    this->last_response_size_ = response_pdu.size();
  }
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_count_++;
    return false;
  }
  int sent_count_{0};
  int response_count_{0};
  int no_response_count_{0};
  size_t last_response_size_{0};
};
}  // namespace

// A broadcast (address 0) is never answered (Modbus 4.1), so the client treats it as fire-and-forget:
// on_sent fires as the frame goes out, NO terminal (on_response/on_error/on_no_response) is delivered,
// the hub is left NOT waiting - no timeout is burned - and the sweep erases the entry.
TEST(ModbusClientHubBroadcast, CompletesAtTransmissionWithoutWaiting) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  BroadcastProbeDevice device(&hub, BROADCAST_ADDRESS);

  const uint8_t write[] = {0x06, 0x00, 0x10, 0x00, 0x01};  // write single register 0x0010 = 0x0001
  ASSERT_TRUE(device.queue_pdu(write));
  EXPECT_EQ(hub.queued_frames(), 1u);

  hub.send_next_for_test();  // transmit + sweep

  EXPECT_EQ(device.sent_count_, 1);         // the frame went on the wire
  EXPECT_EQ(device.response_count_, 0);     // fire-and-forget: no terminal callback
  EXPECT_EQ(device.no_response_count_, 0);  // and it never waited for a reply
  EXPECT_FALSE(hub.waiting());              // no waiting slot occupied
  EXPECT_EQ(hub.queued_frames(), 0u);       // and the entry is gone
  EXPECT_EQ(hub.entries(), 0u);
}

namespace {
// Keeps the DEFAULT on_response() (so the base typed dispatcher runs) and records the typed write
// callback and the catch-all, to prove a broadcast reaches neither - only on_sent.
class BroadcastTypedProbeDevice : public ModbusClientDevice {
 public:
  BroadcastTypedProbeDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_sent(std::span<const uint8_t> request_pdu) override { this->sent_count_++; }
  void on_write_single_register(uint16_t address, uint16_t value, ResponseStatus status) override {
    this->write_single_count_++;
  }
  void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          ResponseStatus status) override {
    this->custom_count_++;
  }
  int sent_count_{0};
  int write_single_count_{0};
  int custom_count_{0};
};
}  // namespace

// Completing a broadcast with an empty response({}) used to fall, for a device on the default
// on_response(), through the typed dispatcher to on_custom_response() - firing the wrong callback and
// logging a spurious "non-standard" warning. Fire-and-forget delivers no terminal at all, so a broadcast
// write reaches neither the typed write callback nor the catch-all: only on_sent.
TEST(ModbusClientHubBroadcast, DeliversNoTerminalToTypedDevice) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  BroadcastTypedProbeDevice device(&hub, BROADCAST_ADDRESS);

  const uint8_t write[] = {0x06, 0x00, 0x10, 0x00, 0x01};  // write single register 0x0010 = 0x0001
  ASSERT_TRUE(device.queue_pdu(write));

  hub.send_next_for_test();  // transmit + sweep

  EXPECT_EQ(device.sent_count_, 1);          // on_sent still reports the transmission
  EXPECT_EQ(device.write_single_count_, 0);  // no terminal: the typed write callback never fires
  EXPECT_EQ(device.custom_count_, 0);        // and it is NOT diverted to the catch-all (no false warning)
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);
}

// A broadcast is only meaningful for a command that changes state; a broadcast READ could never be
// answered, so the hub refuses it at the door (false return, no entry queued) rather than silently
// retiring it. Writes, 0x17, and custom codes still go through (covered above).
TEST(ModbusClientHubBroadcast, RefusesReadBroadcast) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  BroadcastProbeDevice device(&hub, BROADCAST_ADDRESS);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x02};  // read holding registers 0x0010, count 2
  EXPECT_FALSE(device.queue_pdu(read));                   // refused: a broadcast read is never answered
  EXPECT_EQ(hub.entries(), 0u);                           // nothing entered the machine
  EXPECT_FALSE(hub.waiting());

  hub.send_next_for_test();          // nothing to send
  EXPECT_EQ(device.sent_count_, 0);  // never transmitted
}

// The counterpart to RefusesReadBroadcast: a custom (user-defined) function code carries no reply the
// hub knows how to expect, so a broadcast of one is accepted and completes fire-and-forget like a write.
TEST(ModbusClientHubBroadcast, AcceptsCustomBroadcast) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  BroadcastProbeDevice device(&hub, BROADCAST_ADDRESS);

  const uint8_t custom[] = {0x41, 0x01, 0x02};  // FC 0x41: first user-defined function code space
  ASSERT_TRUE(device.queue_pdu(custom));        // accepted: a custom code is not a read
  EXPECT_EQ(hub.queued_frames(), 1u);

  hub.send_next_for_test();  // transmit + sweep

  EXPECT_EQ(device.sent_count_, 1);         // the frame went on the wire
  EXPECT_EQ(device.response_count_, 0);     // fire-and-forget: no terminal callback
  EXPECT_EQ(device.no_response_count_, 0);  // and it never waited for a reply
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);  // the entry is gone
}

// An exception-flagged custom code (0x80 bit set) is not a real request: is_function_code_custom() masks
// the bit away and would accept it, but the broadcast guard excludes it, matching classify()'s handling
// of an exception-flagged write.
TEST(ModbusClientHubBroadcast, RefusesExceptionFlaggedCustomBroadcast) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  BroadcastProbeDevice device(&hub, BROADCAST_ADDRESS);

  const uint8_t exception_custom[] = {0xC1, 0x01, 0x02};  // 0x41 | 0x80: custom code with the exception bit
  EXPECT_FALSE(device.queue_pdu(exception_custom));       // refused: exception-flagged, never a real broadcast
  EXPECT_EQ(hub.entries(), 0u);                           // nothing entered the machine
  EXPECT_FALSE(hub.waiting());

  hub.send_next_for_test();          // nothing to send
  EXPECT_EQ(device.sent_count_, 0);  // never transmitted
}

namespace {
// tx_blocked() clear for send_next_frame_'s gate, then blocked for send_frame_'s post-delay re-check.
class RejectPostDelayHub : public NoResponseProbeHub {
 public:
  bool tx_blocked() override { return this->tx_blocked_calls_++ > 0; }
  int tx_blocked_calls_{0};
};
}  // namespace

// A byte arriving during send_frame_'s pre-send delay blocks transmission after the caller's gate
// already passed. send_frame_ rejects, and send_next_frame_ leaves the frame READY to retry - it is
// not marked WAITING and the bus is not claimed.
TEST(ModbusClientHubSent, SendRejectedAfterDelayLeavesFrameReady) {
  NullUART uart;
  RejectPostDelayHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  SentCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.send_next_for_test();  // gate passes, send_frame_ rejects on the post-delay re-check

  EXPECT_EQ(device.sent_count_, 0);  // nothing transmitted
  EXPECT_FALSE(hub.waiting());       // the frame was left untouched, bus not claimed
  ASSERT_EQ(hub.entries(), 1u);
  EXPECT_EQ(hub.queued(0).state, FrameState::READY);  // still selectable next loop
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
    hub.force_send_next();
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

  device.queue_pdu(read_pdu());
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

  device.queue_pdu(read_pdu());
  device.queue_pdu(read_pdu());
  int cycles = drain_with_responses(hub, OK_RESPONSE);

  EXPECT_EQ(cycles, 2);
  EXPECT_EQ(device.data_count_, 2);
  EXPECT_EQ(device.not_sent_count_, 0);  // both requests were served
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// Clears the address queue from inside its first response callback, so a duplicate still owed on the
// same entry has to be resolved (or, as things stand, is dropped) by that clear.
class ClearOnFirstResponseDevice : public DataCountingDevice {
 public:
  ClearOnFirstResponseDevice(ModbusClientHub *hub, uint8_t address) : DataCountingDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->data_count_++;
    if (this->data_count_ == 1)
      this->clear_tx_queue_for_address();  // clear mid-completion, from inside the first response
  }
};
}  // namespace

// A duplicate read absorbs into one entry (pending 2). The first response resolves one request, and
// its callback clears the address queue mid-completion. The still-owed duplicate is a second accepted
// request, so it must get its own terminal - on_not_sent() - not be dropped silently.
TEST(ModbusClientHubCallbackCount, ClearFromResponseResolvesDuplicateWithNotSent) {
  NoResponseProbeHub hub;
  ClearOnFirstResponseDevice device(&hub, 0x02);

  EXPECT_TRUE(device.queue_pdu(read_pdu()));
  EXPECT_TRUE(device.queue_pdu(read_pdu()));  // absorbed: one entry, pending 2
  hub.force_send_next();
  hub.receive_frame_for_test(0x02, OK_RESPONSE);  // response -> on_response -> clear, then sweep

  EXPECT_EQ(device.data_count_, 1);      // exactly one response delivered
  EXPECT_EQ(device.not_sent_count_, 1);  // the duplicate resolved with a terminal, not dropped
  EXPECT_EQ(device.terminals(), 2);      // one terminal per accepted request
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A read entry serves two requests (this run plus one re-run), so the third identical request is
// refused at the door: two data callbacks, and no terminal for the request that was never taken.
TEST(ModbusClientHubCallbackCount, TripleReadRefusesTheThird) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  EXPECT_TRUE(device.queue_pdu(read_pdu()));
  EXPECT_TRUE(device.queue_pdu(read_pdu()));
  EXPECT_FALSE(device.queue_pdu(read_pdu()));  // the entry is already at its cap
  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 0);  // refused synchronously, nothing owed
  int cycles = drain_with_responses(hub, OK_RESPONSE);

  EXPECT_EQ(cycles, 2);
  EXPECT_EQ(device.data_count_, 2);
  EXPECT_EQ(device.terminals(), 2);  // exactly one per accepted request
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A duplicate write is refused at the door and the original write sends once - the caller learns
// immediately, and no lifecycle is created for the request that was never taken.
TEST(ModbusClientHubCallbackCount, DuplicateWriteRefusedWithoutLifecycle) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  EXPECT_TRUE(device.queue_pdu(write_pdu));
  EXPECT_FALSE(device.queue_pdu(write_pdu));
  hub.sweep_for_test();

  EXPECT_EQ(device.terminals(), 0);  // the accepted write has not resolved; the other never existed
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).priority(), CommandPriority::WRITE);
}

// An exception response is a terminal on its own: exactly one on_error(), no others,
// preceded by exactly one on_sent().
TEST(ModbusClientHubCallbackCount, ErrorResponseIsSoleTerminal) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
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

  device.queue_pdu(read_pdu());
  hub.send_next_for_test();
  hub.timeout_waiting();
  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.terminals(), 1);
  EXPECT_EQ(device.sent_count_, 1);

  // Unabsorbable duplicate: the second identical write is refused at the door - no lifecycle, no
  // terminal, nothing sent.
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  EXPECT_TRUE(device.queue_pdu(write_pdu));
  EXPECT_FALSE(device.queue_pdu(write_pdu));
  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 0);
  EXPECT_EQ(device.terminals(), 1);  // still just the read's timeout
  EXPECT_EQ(device.sent_count_, 1);

  // Drain the accepted write: its echo response is the data terminal, and the books balance.
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, write_pdu);
  EXPECT_EQ(device.data_count_, 1);
  EXPECT_EQ(device.terminals(), 2);  // 2 accepted lifecycles, 2 terminals
  EXPECT_EQ(device.sent_count_, 2);  // 2 transmissions; the refused duplicate never sent
}

// A device-requested retry starts a new lifecycle: each transmission gets its own sent + terminal.
TEST(ModbusClientHubCallbackCount, RetryLifecyclesEachGetSentAndTerminal) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);
  device.retries_ = 1;  // ask for exactly one retry

  device.queue_pdu(read_pdu());
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

// A retry is a state flip on an existing entry, never a new insertion, so a full queue can't refuse
// it: fill the queue, time out the waiting frame with a retry, and it survives as READY.
TEST(ModbusClientHubCallbackCount, RetryIsNeverRefusedByFullQueue) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);
  device.retries_ = 1;
  SentCountingDevice filler(&hub, 0x05);

  device.queue_pdu(read_pdu());
  hub.force_send_next();         // waiting
  device.queue_pdu(read_pdu());  // absorbed: two requests pending
  // Fill the remaining live capacity with distinct frames.
  for (uint16_t i = 0; hub.entries() < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.queue_pdu(fill);
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

// The deprecated device-side send_raw() reports an unusable payload the same way every other
// refused send does: false at the call site, with no queue entry and no callback.
namespace {
class NotSentCountingRawDevice : public ModbusClientDevice {
 public:
  NotSentCountingRawDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_count_++; }
  int not_sent_count_{0};
};
}  // namespace

TEST(ModbusClientHubQueue, SendRawTooShortIsRefusedAtTheDoor) {
  NoResponseProbeHub hub;
  NotSentCountingRawDevice device(&hub, 0x02);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  device.send_raw({});  // too short to contain a PDU; the deprecated void spelling cannot report it
#pragma GCC diagnostic pop
  EXPECT_EQ(device.not_sent_count_, 0);  // refused at the door: no callback delivered
  EXPECT_TRUE(hub.tx_buffer_empty());    // the only evidence of the refusal is that nothing queued
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
      this->queue_pdu(follow);
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
  controller_like.queue_pdu(read_a);
  bystander_same.queue_pdu(read_b);
  bystander_other.queue_pdu(read_c);
  ASSERT_EQ(hub.queued_frames(), 3u);

  controller_like.clear_tx_queue_for_address();
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
// books balance for owners counting outstanding requests - all within the one sweep.
TEST(ModbusClientHubQueue, ClearAddressDeliversOneTerminalPerAcceptedRequest) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  device.queue_pdu(read);
  device.queue_pdu(read);  // duplicate: absorbed into the queued entry
  ASSERT_EQ(hub.queued_frames(), 1u);
  ASSERT_EQ(hub.queued(0).pending, 2u);

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 2);  // one terminal per accepted request
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(hub.entries(), 0u);  // fully drained and erased
}

// A duplicate read absorbs into one waiting entry (pending 2 once the first is sent). A clear with
// clear_sent detaches the in-flight frame as a silent shell, but the duplicate - a second accepted
// request that would have re-run - was never transmitted, so it must still get its on_not_sent().
TEST(ModbusClientHubQueue, ClearSentOnInFlightDuplicateStillNotifiesTheDuplicate) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  device.queue_pdu(read_pdu());  // absorbed: one entry, pending 2
  ASSERT_EQ(hub.queued(0).pending, 2u);
  hub.force_send_next();  // the frame is sent (WAITING); pending still 2
  ASSERT_TRUE(hub.waiting());

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 1);  // the un-transmitted duplicate is resolved, not dropped
}

// A clear does not abandon the in-flight frame: it becomes a WAITING_RETIRED shell that keeps the
// bus and still delivers the in-flight request's usual callback (here on_response) when the reply
// arrives. Only un-run duplicates are turned into on_not_sent(); a lone in-flight frame has none.
TEST(ModbusClientHubQueue, ClearWhileInFlightStillDeliversTheResponse) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();  // sent, now WAITING
  ASSERT_TRUE(hub.waiting());

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 0);  // no un-run duplicate to resolve
  ASSERT_TRUE(hub.waiting());            // still waiting for a response, holding the bus
  ASSERT_EQ(hub.entries(), 1u);          // entry preserved as a cleared shell
  EXPECT_EQ(hub.waiting_command().state, FrameState::WAITING_RETIRED);

  // the in-flight request still gets its usual callback when the response finally arrives
  hub.receive_frame_for_test(0x02, OK_RESPONSE);
  EXPECT_EQ(device.data_count_, 1);
  EXPECT_EQ(device.not_sent_count_, 0);
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);
}

namespace {
// Re-sends its frame once from inside on_not_sent - the re-queued frame must survive the sweep.
class ResendOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t again[] = {0x06, 0x00, 0x40, 0x00, 0x01};  // a write: ranked first at selection, not by position
      this->queue_pdu(again);
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
  device.queue_pdu(read);
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.clear_tx_queue_for_address(0x02);
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
  resender.queue_pdu(read_victim);
  bystander_other.queue_pdu(read_other);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();

  EXPECT_EQ(resender.not_sent_count_, 1);  // notified once, never re-notified for the re-send
  ASSERT_EQ(hub.queued_frames(), 2u);      // the re-queued write AND the other-address read survive
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.address(), 0x02);  // the WRITE class wins selection over the older read
  EXPECT_EQ(next->frame.pdu()[0], 0x06);
}

namespace {
// Re-sends its own frame from EVERY on_not_sent. There is no serve/absorb treadmill: a duplicate at
// the servable cap is refused at the door, and a re-send issued while the entry is retiring queues a
// fresh entry beyond the sweep's captured work_set (served next sweep), never re-absorbing the one draining.
class AlwaysResendDevice : public ModbusClientDevice {
 public:
  AlwaysResendDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    const uint8_t again[] = {0x03, 0x00, 0x50, 0x00, 0x01};
    this->queue_pdu(again);
  }
  int not_sent_count_{0};
};

// From inside on_not_sent, clears ANOTHER address - those victims must still be notified. Nothing
// suppresses that: a re-entrant clear only flips states, retire() is a no-op on an already-retired
// entry, and each entry still owes one notification per un-run request until pending reaches zero.
class ClearOtherOnNotSentDevice : public ModbusClientDevice {
 public:
  ClearOtherOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->parent_->clear_tx_queue_for_address(0x03);
  }
  int not_sent_count_{0};
};
}  // namespace

// pending can never exceed what the entry can serve, so the old serve/absorb treadmill is
// impossible by construction: the surplus request is refused at the door instead of being absorbed
// and resolved later, and a handler that re-sends gets false rather than another lifecycle.
TEST(ModbusClientHubQueue, PendingNeverExceedsTheServableCap) {
  NoResponseProbeHub hub;
  AlwaysResendDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x50, 0x00, 0x01};
  EXPECT_TRUE(device.queue_pdu(read));
  EXPECT_TRUE(device.queue_pdu(read));
  EXPECT_FALSE(device.queue_pdu(read));  // at the cap: refused
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 2u);

  hub.sweep_for_test();  // nothing is owed, so the handler never runs

  EXPECT_EQ(device.not_sent_count_, 0);
  EXPECT_EQ(hub.queued(0).pending, 2u);
}

// A full queue refuses at the door: false at the call site, no entry, no callback - so the
// refusal cannot re-enter the hub at all and needs no recursion bound of its own.
TEST(ModbusClientHubQueue, FullQueueRefusesWithoutCallbacks) {
  NoResponseProbeHub hub;
  SentCountingDevice filler(&hub, 0x05);
  SentCountingDevice device(&hub, 0x02);

  // Fill the queue with distinct frames (distinct start addresses keep the dedup from absorbing them).
  for (uint16_t i = 0; i < MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t fill[] = {0x03, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i & 0xFF), 0x00, 0x01};
    filler.queue_pdu(fill);
  }
  ASSERT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  EXPECT_FALSE(device.queue_pdu(read));  // refused synchronously
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 0);  // nothing was accepted, so nothing is owed
  EXPECT_EQ(hub.queued_frames(), MODBUS_TX_BUFFER_SIZE);
  EXPECT_EQ(hub.entries(), MODBUS_TX_BUFFER_SIZE);  // and no refusal bookkeeping was stored
}

namespace {
// From inside on_not_sent, clears its OWN address - its remaining queued frames resolve silently
// (the guard suppresses self-deliveries), while other owners on the address are still notified.
class ClearOwnAddressOnNotSentDevice : public ModbusClientDevice {
 public:
  ClearOwnAddressOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    this->clear_tx_queue_for_address();
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
  clearer.queue_pdu(read_a);
  clearer.queue_pdu(read_b);
  bystander.queue_pdu(read_c);
  ASSERT_EQ(hub.queued_frames(), 3u);

  EXPECT_FALSE(clearer.queue_pdu(std::span<const uint8_t>{}));  // empty: refused, no callback
  clearer.clear_tx_queue_for_address();                         // the clear the handler used to make

  hub.sweep_for_test();

  EXPECT_EQ(clearer.not_sent_count_, 2);    // one per cleared request of its own
  EXPECT_EQ(bystander.not_sent_count_, 1);  // the bystander's cleared frame is notified too
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(hub.entries(), 0u);
}

// A clear issued from inside on_not_sent() still delivers its victims' notifications in the same sweep:
// the newly-retired entries set sweep_needed_ and the sweep's restart loop drains them before it ends.
TEST(ModbusClientHubQueue, NestedClearFromNotSentStillNotifiesVictims) {
  NoResponseProbeHub hub;
  ClearOtherOnNotSentDevice clearer(&hub, 0x02);
  SentCountingDevice victim(&hub, 0x03);

  const uint8_t read_a[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  const uint8_t read_b[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  clearer.queue_pdu(read_a);
  victim.queue_pdu(read_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02);  // clearer's on_not_sent clears address 0x03 in turn
  hub.sweep_for_test();

  EXPECT_EQ(clearer.not_sent_count_, 1);
  EXPECT_EQ(victim.not_sent_count_, 1);  // the nested clear's victim resolves in the same sweep
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// From on_not_sent (delivered by the sweep), re-sends a frame identical to ANOTHER doomed queued
// frame; the dedup must not absorb into the doomed entry.
class ResendSecondFrameDevice : public ModbusClientDevice {
 public:
  ResendSecondFrameDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t same_as_r2[] = {0x03, 0x00, 0x22, 0x00, 0x01};
      this->queue_pdu(same_as_r2);
    }
  }
  int not_sent_count_{0};
};
}  // namespace

// A send during a sweep that matches a DELETED (doomed) frame must queue fresh, not absorb into
// the doomed entry - absorption would tie the new request to a frame the sweep is draining.
TEST(ModbusClientHubQueue, SweepDedupSkipsDeletedFrames) {
  NoResponseProbeHub hub;
  ResendSecondFrameDevice device(&hub, 0x02);

  const uint8_t r1[] = {0x03, 0x00, 0x21, 0x00, 0x01};
  const uint8_t r2[] = {0x03, 0x00, 0x22, 0x00, 0x01};
  device.queue_pdu(r1);
  device.queue_pdu(r2);
  ASSERT_EQ(hub.queued_frames(), 2u);

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();  // r1 and r2 both resolve; r1's handler re-sends a frame identical to r2
  // Without the dedup's dead-state skip the re-send would be absorbed into r2 and drained with it;
  // with the skip it queues fresh and survives.

  EXPECT_EQ(device.not_sent_count_, 2);  // r1 and r2 both resolved
  ASSERT_EQ(hub.queued_frames(), 1u);    // the re-send survives
  EXPECT_EQ(hub.queued(0).frame.pdu()[2], 0x22);
}

namespace {
// The worst-case handler: from every on_not_sent() it both re-sends and clears its own address, so
// each delivery manufactures a fresh entry AND a fresh terminal debt.
class ResendAndClearOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendAndClearOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    const uint8_t again[] = {0x03, 0x00, 0x70, 0x00, 0x01};
    this->queue_pdu(again);
    this->clear_tx_queue_for_address();
  }
  int not_sent_count_{0};
};
}  // namespace

// Sweep-termination worst case: a handler re-sending AND clearing from every on_not_sent() still
// can't extend the sweep, since it serves only the entries it started with (new debt waits).
TEST(ModbusClientHubQueue, ResendAndClearFromNotSentCannotExtendTheSweep) {
  NoResponseProbeHub hub;
  ResendAndClearOnNotSentDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x70, 0x00, 0x01};
  device.queue_pdu(read);
  hub.clear_tx_queue_for_address(0x02);

  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 1);  // exactly the one terminal that was owed on entry
  EXPECT_EQ(hub.entries(), 1u);          // the frame the handler queued (and then cleared itself)

  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 2);  // its terminal comes on the next loop, not this sweep
  EXPECT_EQ(hub.entries(), 1u);          // and the container is still not growing

  hub.sweep_for_test();
  EXPECT_EQ(device.not_sent_count_, 3);
  EXPECT_EQ(hub.entries(), 1u);
}

// clear_tx_queue_for_device() drops queued frames SILENTLY - no terminal callback (the documented
// exception to the exactly-one-terminal contract; used during teardown/offline handling).
TEST(ModbusClientHubQueue, ClearDeviceQueueDropsSilently) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t read_a[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  const uint8_t read_b[] = {0x03, 0x02, 0x00, 0x00, 0x02};
  device.queue_pdu(read_a);
  device.queue_pdu(read_b);
  ASSERT_EQ(hub.queued_frames(), 2u);

  device.clear_tx_queue_for_device();

  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_EQ(device.not_sent_count_, 0);  // silent drop: no terminal callback
}

// A queue_pdu() from inside on_sent() enqueues behind the waiting frame rather than sending
// immediately or corrupting the waiting transaction.
TEST(ModbusClientHubSent, ReentrantSendFromOnSentQueues) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  ChainOnSentDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.send_next_for_test();  // first frame is sent -> on_sent chains a follow-up

  EXPECT_TRUE(hub.waiting());                     // first frame is waiting
  ASSERT_EQ(hub.queued_frames(), 1u);             // the follow-up queued behind it, not sent
  EXPECT_EQ(hub.queued(0).frame.pdu()[2], 0x09);  // it is the chained read (start address 0x0009)
}

// "Stop polling now" from inside on_response() works: the completing command is exposed to the
// clear routines, which detach it, cancelling the pending continuous re-queue.
TEST(ModbusClientHubPriority, ClearDeviceDuringDataCancelsContinuousRequeue) {
  NoResponseProbeHub hub;
  ClearOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_next();
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
    this->clear_tx_queue_for_address();
  }
};
}  // namespace

// The address-scoped clear cancels the mid-completion re-queue the same way the device-scoped one does.
TEST(ModbusClientHubPriority, ClearAddressDuringDataCancelsContinuousRequeue) {
  NoResponseProbeHub hub;
  ClearAddressOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_next();
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

// send_pdu() was renamed queue_pdu() because the call queues a request rather than transmitting one.
// The old spelling stays for the deprecation window with the signature 2026.7.4 shipped - void, no
// CommandOptions - so a component built against a real release still compiles and still queues.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
TEST(ModbusClientHubCompat, DeprecatedSendPduStillQueues) {
  NoResponseProbeHub hub;
  RetryingDevice device(&hub, 0x02, /*retry=*/false);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.send_pdu(read);  // deprecated device spelling: void, as 2026.7.4 shipped it
  EXPECT_EQ(hub.queued_frames(), 1u);

  // A refusal is invisible to this spelling - no return value and no callback - so the only evidence
  // is that nothing was queued. Reporting the refusal is exactly what moving to queue_pdu() buys.
  device.send_pdu(std::span<const uint8_t>());
  EXPECT_EQ(hub.queued_frames(), 1u);

  // The deprecated hub spelling queues the same way, addressed explicitly.
  const uint8_t other[] = {0x03, 0x00, 0x20, 0x00, 0x01};
  hub.send_pdu(0x03, other, &device);
  EXPECT_EQ(hub.queued_frames(), 2u);

  // Both frames resolve to the same owner. Drain them in turn: the device-spelling frame first (FIFO),
  // then the hub-spelling frame - addressed to 0x03 yet owned by &device, so reaching device's
  // on_no_response proves the request routes by owner pointer, not by address.
  hub.force_send_next();
  hub.timeout_waiting();
  EXPECT_EQ(device.no_response_count_, 1);  // device-spelling frame (address 0x02)
  hub.force_send_next();
  hub.timeout_waiting();
  EXPECT_EQ(device.no_response_count_, 2);  // hub-spelling frame (address 0x03, &device routing)
}
#pragma GCC diagnostic pop

TEST(ModbusClientHubCompat, LegacyCallbackNamesStillForward) {
  NoResponseProbeHub hub;
  LegacyNameDevice device(&hub, 0x02);

  const uint8_t read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.queue_pdu(read);
  hub.force_send_next();
  hub.timeout_waiting();  // no reply -> on_no_response -> forwards to on_modbus_no_response
  EXPECT_EQ(device.legacy_no_response_, 1);

  // A refused send returns false with no callback, so exercise the forward through an accepted
  // request instead: a cleared queue entry delivers on_not_sent(), which forwards to the old name.
  EXPECT_FALSE(device.queue_pdu(std::span<const uint8_t>()));  // empty PDU: refused at the door
  EXPECT_EQ(device.legacy_not_sent_, 0);
  const uint8_t queued[] = {0x03, 0x00, 0x11, 0x00, 0x01};
  EXPECT_TRUE(device.queue_pdu(queued));
  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();
  EXPECT_EQ(device.legacy_not_sent_, 1);
}

// The queue_pdu() capacity bound: a PDU larger than MAX_PDU_SIZE would build a frame past the RTU
// 256-byte limit, so it is refused up front - false at the call site, no entry, no callback.
TEST(ModbusClientHub, OversizedPduIsRefusedAtTheDoor) {
  NoResponseProbeHub hub;
  LegacyNameDevice device(&hub, 0x02);
  std::vector<uint8_t> big(MAX_PDU_SIZE + 1, 0x41);
  EXPECT_FALSE(device.queue_pdu(big));
  EXPECT_EQ(device.legacy_not_sent_, 0);  // refusals are returned, never delivered
  EXPECT_TRUE(hub.tx_buffer_empty());
  EXPECT_EQ(hub.entries(), 0u);
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
  device.queue_pdu(read_req);
  hub.force_send_next();
  const uint8_t response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, response);
  const std::vector<uint8_t> expected{0x00, 0x2A, 0x01, 0x00};
  EXPECT_EQ(device.last_data_, expected);

  // Write echo: no byte-count byte, so the payload is everything after the function code.
  const uint8_t write_req[] = {0x06, 0x00, 0x10, 0x00, 0x2A};
  device.queue_pdu(write_req);
  hub.force_send_next();
  hub.receive_frame_for_test(0x02, write_req);  // single-write responses echo the request
  const std::vector<uint8_t> expected_echo{0x00, 0x10, 0x00, 0x2A};
  EXPECT_EQ(device.last_data_, expected_echo);

  // Exception response: on_modbus_error() received the masked function code and the exception code.
  device.queue_pdu(read_req);
  hub.force_send_next();
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
    hub.force_send_next();
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
  hub.force_send_next();
  hub.timeout_waiting();

  device.read_entities(EntityType::DISCRETE_INPUT, 0x0001, 1);
  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).frame.pdu()[0], 0x02);
  hub.force_send_next();
  hub.timeout_waiting();

  device.read_entities(EntityType::CUSTOM, 0x0001, 1);  // no read function: logged and not queued
  EXPECT_EQ(hub.queued_frames(), 0u);
}

// A rejected read_entities() returns false like every other refused send.
namespace {
class NotSentCountingDevice : public ModbusClientDevice {
 public:
  NotSentCountingDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_++; }
  int not_sent_{0};
};
}  // namespace

TEST(ModbusTypedSendHelpers, InvalidReadEntitiesIsRefusedAtTheDoor) {
  NoResponseProbeHub hub;
  NotSentCountingDevice device(&hub, 0x02);
  EXPECT_FALSE(device.read_entities(EntityType::CUSTOM, 0x0001, 1));
  EXPECT_EQ(device.not_sent_, 0);  // refused sends report through the return value
  EXPECT_EQ(hub.queued_frames(), 0u);
}

namespace {
// Re-sends its own frame from inside on_response() - matching the command mid-completion.
class ResendOnDataDevice : public ModbusClientDevice {
 public:
  ResendOnDataDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->queue_pdu(std::vector<uint8_t>(request_pdu.begin(), request_pdu.end()));
  }
  void queue_pdu(const std::vector<uint8_t> &pdu) { ModbusClientDevice::queue_pdu(pdu); }
};
}  // namespace

// A send from inside on_response() that matches the RECEIVED (completing) entry is absorbed into it,
// never a fresh twin. Here it is a one-shot re-send of a continuous poll, so it also downgrades the
// poll to a one-shot: one entry on the queue afterwards, now non-continuous.
TEST(ModbusClientHubPriority, ResendFromOnResponseAbsorbsIntoCompletingCommand) {
  NoResponseProbeHub hub;
  ResendOnDataDevice device(&hub, 0x02);

  device.read_holding_registers(0x100, 2, {.continuous = true});
  hub.force_send_next();
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);  // handler re-sends the identical frame mid-completion

  ASSERT_EQ(hub.queued_frames(), 1u);              // absorbed into the same entry, not a fresh twin
  EXPECT_FALSE(hub.queued(0).options.continuous);  // the one-shot re-send downgraded the poll
}

// An exception-flagged function code is never silently re-sendable, even though the read check
// masks the exception bit: its duplicate takes the drop path like any other non-read.
TEST(ModbusClientHubPriority, ExceptionFlaggedDuplicateDroppedNotPromoted) {
  NoResponseProbeHub hub;
  SentCountingDevice device(&hub, 0x02);

  const uint8_t weird[] = {0x83, 0x01, 0x00, 0x00, 0x02};  // read-shaped but exception-flagged
  EXPECT_TRUE(device.queue_pdu(weird));
  EXPECT_FALSE(device.queue_pdu(weird));  // non-requeueable: cap of one, so the duplicate is refused
  hub.sweep_for_test();

  ASSERT_EQ(hub.queued_frames(), 1u);
  EXPECT_EQ(hub.queued(0).pending, 1u);
  EXPECT_EQ(device.not_sent_count_, 0);

  // The write-shaped twin (0x86 masks to WRITE_SINGLE_REGISTER) must not take WRITE-class
  // ordering either: exception-flagged codes are excluded from the mutates classification.
  const uint8_t weird_write[] = {0x86, 0x00, 0x10, 0xBE, 0xEF};
  device.queue_pdu(weird_write);
  ASSERT_EQ(hub.queued_frames(), 2u);
  EXPECT_EQ(hub.queued(1).priority(), CommandPriority::READ);  // not WRITE
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->frame.pdu()[0], 0x83);  // FIFO by age: it did not jump the older entry
}

namespace {
// From inside the sweep's on_not_sent, re-sends the frame that is currently WAITING.
class ResendInFlightOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendInFlightOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t same_as_waiting[] = {0x03, 0x01, 0x00, 0x00, 0x02};  // == READ_PDU
      this->queue_pdu(same_as_waiting);
    }
  }
  int not_sent_count_{0};
};
}  // namespace

// A clear turns the waiting entry into a WAITING_RETIRED shell. The shell keeps its device (so the
// in-flight request still gets its callback), but the dedup skips it, so a sweep handler re-sending
// that frame queues fresh instead of being absorbed into the cleared shell and drained as on_not_sent.
TEST(ModbusClientHubQueue, SweepResendAfterClearQueuesFreshNotAbsorbedIntoShell) {
  NoResponseProbeHub hub;
  ResendInFlightOnNotSentDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();  // READ_PDU now waiting
  const uint8_t queued_read[] = {0x03, 0x00, 0x10, 0x00, 0x01};
  device.queue_pdu(queued_read);  // a queued frame for the sweep to notify
  ASSERT_EQ(hub.queued_frames(), 1u);

  hub.clear_tx_queue_for_address(0x02);
  hub.sweep_for_test();

  EXPECT_EQ(device.not_sent_count_, 1);  // only the cleared queued frame, not the re-send
  ASSERT_EQ(hub.queued_frames(), 1u);    // the handler's re-send queued fresh...
  EXPECT_EQ(hub.queued(0).pending, 1u);  // ...not absorbed into the cleared shell
  EXPECT_TRUE(std::equal(hub.queued(0).frame.pdu().begin(), hub.queued(0).frame.pdu().end(), READ_PDU));
  EXPECT_EQ(hub.waiting_command().state, FrameState::WAITING_RETIRED);  // in-flight one still awaiting a reply
}

namespace {
// Gives up after a timeout by clearing its address from inside on_no_response() - the natural
// "device is dead, drop my traffic" pattern, and the reentrant case the address clear must handle.
class ClearAddressOnNoResponseDevice : public ModbusClientDevice {
 public:
  ClearAddressOnNoResponseDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  bool on_no_response(std::span<const uint8_t> request_pdu) override {
    this->no_response_count_++;
    this->clear_tx_queue_for_address();
    return false;  // gave up
  }
  void on_not_sent(std::span<const uint8_t> request_pdu) override { this->not_sent_count_++; }
  int terminals() const { return this->no_response_count_ + this->not_sent_count_; }
  int no_response_count_{0};
  int not_sent_count_{0};
};

}  // namespace

// A clear issued from inside on_no_response() must not cause the request to be resolved twice:
// that callback already was its terminal, so the entry it hijacks owes nothing more.
TEST(ModbusClientHubNoResponse, SelfClearFromNoResponseDoesNotDoubleResolve) {
  NoResponseProbeHub hub;
  ClearAddressOnNoResponseDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.terminals(), 1);  // exactly one terminal for the one accepted request
  EXPECT_EQ(hub.entries(), 0u);
}

// The same entry standing for two accepted requests: the timeout resolves one, and the clear that
// cancels the re-run must resolve exactly the other.
TEST(ModbusClientHubNoResponse, SelfClearFromNoResponseResolvesTheAbsorbedRequestOnce) {
  NoResponseProbeHub hub;
  ClearAddressOnNoResponseDevice device(&hub, 0x02);

  EXPECT_TRUE(device.queue_pdu(read_pdu()));
  EXPECT_TRUE(device.queue_pdu(read_pdu()));  // absorbed: one entry, two requests
  hub.force_send_next();
  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_EQ(device.terminals(), 2);  // one per accepted request, no more
  EXPECT_EQ(hub.entries(), 0u);
}

// A cleared in-flight frame must release the bus by both exits and still deliver the in-flight
// request's usual callback (on_response here, on_no_response on timeout); no on_not_sent, no duplicate.
TEST(ModbusClientHubQueue, ClearedShellReleasesTheBusOnLateResponse) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  hub.clear_tx_queue_for_address(0x02);
  ASSERT_EQ(hub.waiting_command().state, FrameState::WAITING_RETIRED);

  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);  // the late reply for the cleared frame

  EXPECT_FALSE(hub.waiting());           // the bus is free again
  EXPECT_EQ(hub.entries(), 0u);          // the shell is gone
  EXPECT_EQ(device.data_count_, 1);      // the in-flight request still got its response callback
  EXPECT_EQ(device.not_sent_count_, 0);  // no un-run duplicate
}

TEST(ModbusClientHubQueue, ClearedShellReleasesTheBusOnTimeout) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  hub.clear_tx_queue_for_address(0x02);
  ASSERT_EQ(hub.waiting_command().state, FrameState::WAITING_RETIRED);

  hub.timeout_waiting();  // no reply ever arrives; the watchdog releases the shell

  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);
  EXPECT_EQ(device.no_response_count_, 1);  // the in-flight request got its on_no_response
  EXPECT_EQ(device.not_sent_count_, 0);     // no un-run duplicate
}

// Clearing an interrupted (not-yet-notified) frame keeps its distrust: it becomes an
// INTERRUPTED_RETIRED shell that still ends in on_no_response at the timeout - never delivering a
// late response as on_response. No duplicate here, so no on_not_sent.
TEST(ModbusClientHubQueue, ClearInterruptedFrameGetsNoResponseAtTimeout) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);  // declines the retry (retries_ == 0)

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);  // wrong address: interrupts the transaction
  hub.sweep_for_test();
  ASSERT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED);

  hub.clear_tx_queue_for_address(0x02);
  ASSERT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED_RETIRED);

  // A late MATCHING response is ignored (distrust survives the clear), not delivered as on_response.
  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);
  EXPECT_EQ(device.data_count_, 0);
  ASSERT_TRUE(hub.waiting());  // still held; the ignored response did not free the wire

  hub.timeout_waiting();

  EXPECT_EQ(device.no_response_count_, 1);  // the interrupted request's usual terminal, at the timeout
  EXPECT_EQ(device.not_sent_count_, 0);     // no un-run duplicate
  EXPECT_EQ(hub.queued_frames(), 0u);
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);
}

// The other order: clear a WAITING frame, THEN an unexpected frame arrives. The distrust must still
// take hold - the cleared shell becomes INTERRUPTED_RETIRED and a later matching frame is ignored.
TEST(ModbusClientHubQueue, InterruptAfterClearStillDistrusts) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  hub.clear_tx_queue_for_address(0x02);
  ASSERT_EQ(hub.waiting_command().state, FrameState::WAITING_RETIRED);

  const uint8_t stray_pdu[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, stray_pdu);  // unexpected frame interrupts the cleared shell
  ASSERT_EQ(hub.waiting_command().state, FrameState::INTERRUPTED_RETIRED);

  const uint8_t ok_response[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x02, ok_response);  // now-distrusted late response is ignored
  EXPECT_EQ(device.data_count_, 0);

  hub.timeout_waiting();
  EXPECT_EQ(device.no_response_count_, 1);
  EXPECT_FALSE(hub.waiting());
  EXPECT_EQ(hub.entries(), 0u);
}

// A cleared waiting duplicate (pending 2) that times out: the duplicate drains to on_not_sent and
// the in-flight request gets on_no_response, with nothing re-transmitted (sweep runs before timeout).
TEST(ModbusClientHubQueue, ClearedInFlightDuplicateTimesOutWithoutRerunning) {
  NoResponseProbeHub hub;
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  device.queue_pdu(read_pdu());  // absorbed: one entry, pending 2
  ASSERT_EQ(hub.queued(0).pending, 2u);
  hub.force_send_next();  // sent, pending still 2
  hub.clear_tx_queue_for_address(0x02);

  hub.timeout_waiting();

  EXPECT_EQ(device.not_sent_count_, 1);     // the un-run duplicate
  EXPECT_EQ(device.no_response_count_, 1);  // the in-flight request's usual terminal
  EXPECT_EQ(hub.queued_frames(), 0u);       // nothing re-transmitted
  EXPECT_EQ(hub.entries(), 0u);             // fully drained and erased
  EXPECT_FALSE(hub.waiting());
}

// An absorbed extra request also gets its run after an error response - the re-request was
// explicit, so it runs once more whether this attempt succeeded or not.
TEST(ModbusClientHubCallbackCount, AbsorbedRequestRunsAfterErrorResponse) {
  NullUART uart;
  NoResponseProbeHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();
  DataCountingDevice device(&hub, 0x02);

  device.queue_pdu(read_pdu());
  hub.force_send_next();
  device.queue_pdu(read_pdu());  // waiting duplicate: absorbed
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
  device.queue_pdu(read);
  device.queue_pdu(mask_write);

  ASSERT_EQ(hub.queued_frames(), 2u);
  const ModbusDeviceCommand *next = hub.next_ready();
  ASSERT_NE(next, nullptr);
  EXPECT_EQ(next->priority(), CommandPriority::WRITE);  // 0x16 wins selection over the queued read
  EXPECT_EQ(next->frame.pdu()[0], 0x16);
}
}  // namespace esphome::modbus::testing
