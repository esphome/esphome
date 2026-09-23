#include <gtest/gtest.h>

#include <cstdint>
#include <span>

#include "esphome/components/modbus/modbus.h"

namespace esphome::modbus::testing {

namespace {

// Exposes the protected tx queue and waiting-for-response slot so tests can drive the
// no-response path without a UART: force_send_front() mimics send_next_frame_() moving the
// front frame in flight, timeout_waiting() mimics the loop() no-response timeout handling.
class NoResponseProbeHub : public ModbusClientHub {
 public:
  size_t queued_frames() const { return this->tx_buffer_.size(); }
  const ModbusDeviceCommand &front() const { return this->tx_buffer_.front(); }
  bool waiting() const { return this->waiting_for_response_.has_value(); }
  const ModbusDeviceCommand &waiting_command() const {
    EXPECT_TRUE(this->waiting_for_response_.has_value());
    return *this->waiting_for_response_;  // NOLINT(bugprone-unchecked-optional-access)
  }

  void force_send_front() {
    this->waiting_for_response_ = std::move(this->tx_buffer_.front());
    this->tx_buffer_.pop_front();
  }
  // Drives the real unexpected-frame branch in process_modbus_server_frame().
  void receive_frame_for_test(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len) {
    this->process_modbus_server_frame(address, function_code, data, len);
  }
  void timeout_waiting() {
    if (this->waiting_for_response_.has_value())
      this->notify_no_response_(*this->waiting_for_response_);
    this->waiting_for_response_.reset();
  }
};

// A device with a scripted answer to on_modbus_no_response().
class RetryingDevice : public ModbusClientDevice {
 public:
  RetryingDevice(ModbusClientHub *hub, uint8_t address, bool retry) : ModbusClientDevice(hub, address), retry_(retry) {}
  bool on_modbus_no_response() override {
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
  bool on_modbus_no_response() override {
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
  EXPECT_EQ(requeued.frame.data.data()[0], 0x02);
  EXPECT_EQ(0, memcmp(requeued.frame.data.data() + 1, READ_PDU, sizeof(READ_PDU)));
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
  const uint8_t stray_payload[] = {0x04, 0x00, 0x2A, 0x01, 0x00};
  hub.receive_frame_for_test(0x07, 0x03, stray_payload, sizeof(stray_payload));

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

}  // namespace esphome::modbus::testing
