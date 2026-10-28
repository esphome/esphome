#include <gtest/gtest.h>
#include <array>
#include <deque>
#include <vector>

#include "esphome/components/haier_water_heater/hon_water_heater.h"
#include "esphome/core/application.h"

namespace esphome::haier_water_heater::testing {

class RecordingUART : public uart::UARTComponent {
 public:
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> sent;
  void write_array(const uint8_t *data, size_t size) override { this->sent.emplace_back(data, data + size); }
  bool read_array(uint8_t *data, size_t size) override {
    if (this->rx.size() < size)
      return false;
    for (size_t i = 0; i < size; i++) {
      data[i] = this->rx.front();
      this->rx.pop_front();
    }
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

class TestableHeater : public HonWaterHeater {
 public:
  using HonWaterHeater::poll_status_;
  void set_network_pending(bool value) { this->network_pending_ = value; }
  bool waiting_poll() const { return this->awaiting_ == Awaiting::AWAITING_POLL; }
  bool pending_off() const { return this->pending_off_; }
  uint8_t command_count() const { return this->command_count_; }
  void cancel_test_intervals() {
    this->cancel_interval(NETWORK_STATUS_INTERVAL_ID);
    this->cancel_interval(STATUS_POLL_INTERVAL_ID);
  }
};

class HonControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->heater_.set_uart_parent(&this->uart_);
    this->heater_.set_network_pending(false);
    this->set_clock_(100);
  }
  void TearDown() override { this->heater_.cancel_test_intervals(); }
  void set_clock_(uint32_t ms) { LoopBlockingGuard clock{nullptr, nullptr, ms}; }
  void status_(bool powered, bool unknown = false, bool eco = false, bool bps = false) {
    std::array<uint8_t, 61> core{};
    core[0] = 60;
    core[7] = 2;
    core[8] = 0x6D;
    core[9] = 1;
    core[10] = 42;
    core[11] = 5;
    core[23] = 43;
    core[46] = (unknown || eco || bps) ? 0 : 16;
    core[47] = ((unknown || eco || bps) ? 0 : 16) | (powered ? 1 : 0);
    core[48] = bps ? 4 : 0;
    core[49] = eco ? 4 : 0;
    for (size_t i = 0; i < 60; i++) {
      core[60] += core[i];
      if (core[i] == 0xFF)
        core[60] += 0x55;
    }
    this->uart_.rx.insert(this->uart_.rx.end(), {0xFF, 0xFF});
    for (uint8_t byte : core) {
      this->uart_.rx.push_back(byte);
      if (byte == 0xFF)
        this->uart_.rx.push_back(0x55);
    }
    this->heater_.loop();
  }
  void request_bps_() {
    auto call = this->heater_.make_call();
    call.set_mode(water_heater::WATER_HEATER_MODE_PERFORMANCE);
    call.perform();
    this->heater_.loop();
  }
  void reject_() {
    uint8_t wire[water_heater_protocol::MAX_WIRE_FRAME];
    const size_t count =
        water_heater_protocol::encode_frame(water_heater_protocol::FrameType::FRAME_TYPE_INVALID, nullptr, 0, wire);
    this->uart_.rx.insert(this->uart_.rx.end(), wire, wire + count);
    this->heater_.loop();
  }
  void request_off_() {
    auto call = this->heater_.make_call();
    call.set_on(false);
    call.perform();
  }
  void expect_off_() {
    ASSERT_FALSE(this->uart_.sent.empty());
    const std::vector<uint8_t> expected = {0xFF, 0xFF, 0x0C, 0, 0, 0, 0, 0, 0, 1, 0x5D, 1, 0, 0, 0x6B};
    EXPECT_EQ(this->uart_.sent.back(), expected);
  }
  RecordingUART uart_;
  TestableHeater heater_;
};

TEST_F(HonControllerTest, OffIgnoresTemperatureWithoutStatus) {
  auto call = this->heater_.make_call();
  call.set_mode(water_heater::WATER_HEATER_MODE_OFF);
  call.set_target_temperature(40);
  call.perform();
  this->heater_.loop();
  this->expect_off_();
}

TEST_F(HonControllerTest, OffSurvivesTimeoutAndCancelsQueuedMode) {
  this->status_(false);
  this->request_bps_();
  ASSERT_EQ(this->heater_.command_count(), 2);
  auto call = this->heater_.make_call();
  call.set_on(false);
  call.set_target_temperature(999);
  call.perform();
  EXPECT_TRUE(this->heater_.pending_off());
  EXPECT_EQ(this->heater_.command_count(), 1);
  this->set_clock_(5101);
  this->heater_.loop();
  this->expect_off_();
  EXPECT_FALSE(this->heater_.pending_off());
  EXPECT_EQ(this->uart_.sent.size(), 2);
}

TEST_F(HonControllerTest, ActiveModeIsNotRequestedAgain) {
  this->status_(true, false, false, true);
  this->request_bps_();
  EXPECT_EQ(this->heater_.command_count(), 0);
  EXPECT_TRUE(this->uart_.sent.empty());
  auto call = this->heater_.make_call();
  call.set_mode(water_heater::WATER_HEATER_MODE_ECO);
  call.perform();
  EXPECT_EQ(this->heater_.command_count(), 2);  // through Normal, then ECO
}

TEST_F(HonControllerTest, PowerOnIsNotRequestedAgain) {
  this->status_(true);
  auto call = this->heater_.make_call();
  call.set_on(true);
  call.set_target_temperature(40);
  call.perform();
  EXPECT_EQ(this->heater_.command_count(), 1);  // the temperature only
}

TEST_F(HonControllerTest, OffFollowsConfirmedInFlightCommand) {
  this->status_(false);
  this->request_bps_();
  this->request_off_();
  this->set_clock_(200);
  this->status_(true);
  this->heater_.loop();
  this->expect_off_();
  EXPECT_EQ(this->uart_.sent.size(), 2);
}

TEST_F(HonControllerTest, CommunicationTimeoutAndRecovery) {
  binary_sensor::BinarySensor health;
  this->heater_.set_communication_binary_sensor(&health);
  this->heater_.setup();
  ASSERT_TRUE(health.has_state());
  EXPECT_FALSE(health.state);
  EXPECT_TRUE(this->heater_.status_has_warning());
  this->status_(false);
  EXPECT_TRUE(health.state);
  EXPECT_FALSE(this->heater_.status_has_warning());
  this->set_clock_(20099);
  this->heater_.loop();
  EXPECT_TRUE(health.state);
  this->set_clock_(20100);
  this->heater_.loop();
  EXPECT_FALSE(health.state);
  EXPECT_TRUE(this->heater_.status_has_warning());
  this->heater_.loop();
  EXPECT_FALSE(health.state);
  this->status_(false);
  EXPECT_TRUE(health.state);
  EXPECT_FALSE(this->heater_.status_has_warning());
}

TEST_F(HonControllerTest, ControlFromCommunicationRecoveryIsAccepted) {
  binary_sensor::BinarySensor health;
  this->heater_.set_communication_binary_sensor(&health);
  health.add_on_state_callback([this](bool state) {
    if (!state)
      return;
    EXPECT_TRUE(this->heater_.is_on());
    auto call = this->heater_.make_call();
    call.set_target_temperature(40);
    call.perform();
  });
  this->status_(true);
  EXPECT_EQ(this->heater_.command_count(), 1);
}

TEST_F(HonControllerTest, PollTimeoutReleasesNetworkUpdateThenPollingResumes) {
  this->heater_.poll_status_();
  ASSERT_TRUE(this->heater_.waiting_poll());
  this->heater_.set_network_pending(true);
  this->set_clock_(5099);
  this->heater_.loop();
  ASSERT_EQ(this->uart_.sent.size(), 1);
  this->set_clock_(5100);
  this->heater_.poll_status_();
  this->heater_.loop();
  EXPECT_FALSE(this->heater_.waiting_poll());
  ASSERT_EQ(this->uart_.sent.size(), 2);
  EXPECT_EQ(this->uart_.sent.back()[9], 0xF7);
  this->set_clock_(7201);
  this->heater_.loop();
  this->set_clock_(10100);
  this->heater_.poll_status_();
  EXPECT_TRUE(this->heater_.waiting_poll());
  EXPECT_EQ(this->uart_.sent.back()[9], 0x70);
}

TEST_F(HonControllerTest, OffFollowsPollTimeout) {
  this->heater_.poll_status_();
  this->request_off_();
  this->set_clock_(5100);
  this->heater_.loop();
  this->expect_off_();
}

TEST_F(HonControllerTest, RejectionDropsOnlyTheControlInFlight) {
  this->status_(false);
  this->heater_.poll_status_();
  ASSERT_TRUE(this->heater_.waiting_poll());
  this->request_bps_();
  ASSERT_EQ(this->heater_.command_count(), 2);
  this->reject_();
  EXPECT_TRUE(this->heater_.waiting_poll());
  EXPECT_EQ(this->heater_.command_count(), 2);
  this->set_clock_(5100);
  this->heater_.loop();  // the poll times out and the first queued control goes out
  ASSERT_EQ(this->uart_.sent.size(), 2);
  EXPECT_EQ(this->uart_.sent.back()[10], 0x5D);
  this->reject_();
  EXPECT_EQ(this->heater_.command_count(), 0);
  EXPECT_FALSE(this->heater_.waiting_poll());
}

TEST_F(HonControllerTest, UnknownModeRetainsActiveModeAcrossOff) {
  this->status_(true, true);
  EXPECT_TRUE(this->heater_.is_on());
  EXPECT_EQ(this->heater_.get_mode(), water_heater::WATER_HEATER_MODE_ELECTRIC);
  this->status_(true, false, true);
  EXPECT_EQ(this->heater_.get_mode(), water_heater::WATER_HEATER_MODE_ECO);
  this->status_(false, true);
  EXPECT_EQ(this->heater_.get_mode(), water_heater::WATER_HEATER_MODE_OFF);
  this->status_(true, true);
  EXPECT_TRUE(this->heater_.is_on());
  EXPECT_EQ(this->heater_.get_mode(), water_heater::WATER_HEATER_MODE_ECO);
  this->status_(true);
  EXPECT_EQ(this->heater_.get_mode(), water_heater::WATER_HEATER_MODE_ELECTRIC);
}

TEST_F(HonControllerTest, CustomCommunicationTimeout) {
  binary_sensor::BinarySensor health;
  this->heater_.set_communication_binary_sensor(&health);
  this->heater_.set_communication_timeout(7500);
  this->heater_.setup();
  this->status_(false);
  this->set_clock_(7599);
  this->heater_.loop();
  EXPECT_TRUE(health.state);
  this->set_clock_(7600);
  this->heater_.loop();
  EXPECT_FALSE(health.state);
  this->status_(false);
  EXPECT_TRUE(health.state);
}

}  // namespace esphome::haier_water_heater::testing
