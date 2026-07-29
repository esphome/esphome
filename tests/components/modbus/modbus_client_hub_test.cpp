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

  // A refused send (empty PDU) is a not_sent terminal, never sent.
  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  device.send_pdu(write_pdu);
  device.send_pdu(std::span<const uint8_t>{});
  EXPECT_EQ(device.not_sent_count_, 1);
  EXPECT_EQ(device.terminals(), 2);  // the accepted write is still queued - no terminal for it yet
  EXPECT_EQ(device.sent_count_, 1);  // and it has not transmitted yet

  // Drain it: the write echo response is its data terminal, and the books balance.
  hub.send_next_for_test();
  hub.receive_frame_for_test(0x02, write_pdu);
  EXPECT_EQ(device.data_count_, 1);
  EXPECT_EQ(device.terminals(), 3);  // 3 accepted lifecycles, 3 terminals
  EXPECT_EQ(device.sent_count_, 2);  // 2 transmissions (read + write); the refused send never sent
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

namespace {
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

namespace {
// Re-sends its frame once from inside on_not_sent - the re-queued frame must survive the sweep.
class ResendOnNotSentDevice : public ModbusClientDevice {
 public:
  ResendOnNotSentDevice(ModbusClientHub *hub, uint8_t address) : ModbusClientDevice(hub, address) {}
  void on_not_sent(std::span<const uint8_t> request_pdu) override {
    this->not_sent_count_++;
    if (this->not_sent_count_ == 1) {
      const uint8_t again[] = {0x06, 0x00, 0x40, 0x00, 0x01};
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

// Reacts to a transmit failure by sending another frame from inside the failure callback.
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

}  // namespace

// A transmit failure must resolve with the failed frame OUT of the queue before its on_not_sent runs: a
// handler that reacts by sending a new frame must not have that frame discarded by the pop that
// follows - the failed frame is popped first, the new frame survives.
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
