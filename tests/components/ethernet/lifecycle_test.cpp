#ifdef USE_HOST
#include <gtest/gtest.h>

#include "esphome/components/ethernet/ethernet_component.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"

namespace esphome::ethernet {

namespace testing {
namespace {
struct Driver {
  bool start_ok{true};
  bool stop_ok{true};
  int starts{0};
  int stops{0};
  int connect_attempts{0};
};
Driver driver;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace
}  // namespace testing

// Host implementations of the ESP32-specific hooks.
void EthernetComponent::setup() { this->disabled_ = !this->enable_on_boot_; }
void EthernetComponent::ethernet_lazy_init_() { this->ethernet_initialized_ = true; }
bool EthernetComponent::start_driver_() {
  ++testing::driver.starts;
  return testing::driver.start_ok;
}
bool EthernetComponent::stop_driver_(DriverStopReason) {
  ++testing::driver.stops;
  return testing::driver.stop_ok;
}
void EthernetComponent::start_connect_() {
  ++testing::driver.connect_attempts;
  this->connect_begin_ = App.get_loop_component_start_time();
}
void EthernetComponent::finish_connect_() {}
void EthernetComponent::dump_connect_params_() {}
void EthernetComponent::dump_config() {}
bool EthernetComponent::powerdown() { return true; }
network::IPAddresses EthernetComponent::get_ip_addresses() { return {}; }

namespace testing {

class EthernetLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    driver = {};
    this->ethernet_.set_enable_on_boot(false);
    this->ethernet_.setup();
  }

  void TearDown() override { global_eth_component = nullptr; }

  void tick_() {
    LoopBlockingGuard clock{nullptr, nullptr, ++this->now_};
    this->ethernet_.loop();
  }

  void driver_started_() { this->ethernet_.on_driver_start_(); }
  void driver_stopped_() { this->ethernet_.on_driver_stop_(); }

  void start_connecting_() {
    this->ethernet_.enable();
    this->driver_started_();
    this->tick_();
  }

  void got_ip_() {
    this->ethernet_.connected_ = true;
    this->tick_();
  }

  void lost_link_() {
    this->ethernet_.connected_ = false;
    this->tick_();
  }

  void start_connected_() {
    this->start_connecting_();
    this->got_ip_();
    ASSERT_TRUE(this->ethernet_.is_connected());
  }

  EthernetComponent ethernet_;
  uint32_t now_{0};
};

TEST_F(EthernetLifecycleTest, RejectedStopLeavesConnectionEnabled) {
  this->start_connected_();
  driver.stop_ok = false;

  this->ethernet_.disable();

  EXPECT_EQ(driver.stops, 1);
  EXPECT_TRUE(this->ethernet_.is_enabled());
  EXPECT_TRUE(this->ethernet_.is_connected());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());
}

TEST_F(EthernetLifecycleTest, DriverStopAcknowledgementAndDisconnectPrecedeRestart) {
  this->start_connected_();
  int disconnects = 0;
  LambdaAction<> disconnect{[&] {
    EXPECT_EQ(driver.starts, 1);
    ++disconnects;
  }};
  Automation<> automation{this->ethernet_.get_disconnect_trigger()};
  automation.add_action(&disconnect);

  this->ethernet_.disable();
  EXPECT_TRUE(this->ethernet_.is_disabled());
  EXPECT_FALSE(this->ethernet_.is_connected());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());
  this->ethernet_.enable();
  this->tick_();
  EXPECT_EQ(driver.starts, 1);
  EXPECT_EQ(disconnects, 0);

  this->driver_stopped_();
  EXPECT_TRUE(this->ethernet_.is_driver_stopped());
  this->tick_();
  EXPECT_EQ(disconnects, 1);
  EXPECT_EQ(driver.starts, 2);
  EXPECT_TRUE(this->ethernet_.is_enabled());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());
  this->tick_();
  EXPECT_EQ(driver.starts, 2);
  EXPECT_EQ(disconnects, 1);
}

TEST_F(EthernetLifecycleTest, EnableAfterStopEventStillWaitsForDisconnect) {
  this->start_connected_();
  int disconnects = 0;
  int reconnects = 0;
  LambdaAction<> disconnect{[&] {
    EXPECT_EQ(driver.starts, 1);
    ++disconnects;
  }};
  LambdaAction<> reconnect{[&] { ++reconnects; }};
  Automation<> on_disconnect{this->ethernet_.get_disconnect_trigger()};
  Automation<> on_connect{this->ethernet_.get_connect_trigger()};
  on_disconnect.add_action(&disconnect);
  on_connect.add_action(&reconnect);

  this->ethernet_.disable();
  this->driver_stopped_();
  this->ethernet_.enable();
  EXPECT_EQ(driver.starts, 1);
  this->tick_();
  EXPECT_EQ(disconnects, 1);
  EXPECT_EQ(driver.starts, 2);
  this->driver_started_();
  this->tick_();
  this->got_ip_();
  EXPECT_EQ(reconnects, 1);
  EXPECT_EQ(disconnects, 1);
}

TEST_F(EthernetLifecycleTest, DisableCancelsQueuedRestart) {
  this->start_connected_();
  this->ethernet_.disable();
  this->ethernet_.enable();
  this->ethernet_.disable();
  EXPECT_EQ(driver.stops, 1);

  this->driver_started_();
  this->tick_();
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());
  this->driver_stopped_();
  this->tick_();
  EXPECT_TRUE(this->ethernet_.is_driver_stopped());
  EXPECT_EQ(driver.starts, 1);
}

TEST_F(EthernetLifecycleTest, DisconnectAutomationMayCancelRestart) {
  this->start_connected_();
  LambdaAction<> cancel{[this] { this->ethernet_.disable(); }};
  Automation<> automation{this->ethernet_.get_disconnect_trigger()};
  automation.add_action(&cancel);

  this->ethernet_.disable();
  this->ethernet_.enable();
  this->driver_stopped_();
  this->tick_();

  EXPECT_EQ(driver.starts, 1);
  EXPECT_EQ(driver.stops, 1);
  EXPECT_TRUE(this->ethernet_.is_driver_stopped());
}

TEST_F(EthernetLifecycleTest, DisconnectAutomationMayRestartExactlyOnce) {
  this->start_connected_();
  LambdaAction<> restart{[this] { this->ethernet_.enable(); }};
  Automation<> automation{this->ethernet_.get_disconnect_trigger()};
  automation.add_action(&restart);

  this->ethernet_.disable();
  this->ethernet_.enable();
  this->driver_stopped_();
  this->tick_();
  this->tick_();

  EXPECT_EQ(driver.starts, 2);
  EXPECT_EQ(driver.stops, 1);
  EXPECT_TRUE(this->ethernet_.is_enabled());
}

TEST_F(EthernetLifecycleTest, RestartQueuedByDisconnectAutomationNeedsAnotherStop) {
  this->start_connected_();
  LambdaAction<> restart{[this] {
    this->ethernet_.enable();
    this->ethernet_.disable();
    this->ethernet_.enable();
  }};
  Automation<> automation{this->ethernet_.get_disconnect_trigger()};
  automation.add_action(&restart);

  this->ethernet_.disable();
  this->ethernet_.enable();
  this->driver_stopped_();
  this->tick_();
  this->tick_();
  EXPECT_EQ(driver.starts, 2);
  EXPECT_EQ(driver.stops, 2);
  EXPECT_TRUE(this->ethernet_.is_disabled());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());

  this->driver_stopped_();
  this->tick_();
  EXPECT_EQ(driver.starts, 3);
  EXPECT_TRUE(this->ethernet_.is_enabled());
}

TEST_F(EthernetLifecycleTest, FailedStartCleansUpAndAwaitsStopBeforeRetry) {
  EXPECT_TRUE(this->ethernet_.is_driver_stopped());
  driver.start_ok = false;
  this->ethernet_.enable();
  EXPECT_EQ(driver.starts, 1);
  EXPECT_EQ(driver.stops, 1);
  EXPECT_TRUE(this->ethernet_.is_disabled());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());

  // The driver can post START before returning an error from esp_eth_start().
  this->driver_started_();
  this->tick_();
  EXPECT_EQ(driver.connect_attempts, 0);

  driver.start_ok = true;
  this->ethernet_.enable();
  this->tick_();
  EXPECT_EQ(driver.starts, 1);
  EXPECT_EQ(driver.stops, 1);
  this->driver_stopped_();
  this->tick_();
  EXPECT_EQ(driver.starts, 2);
  EXPECT_TRUE(this->ethernet_.is_enabled());
  this->driver_started_();
  this->tick_();
  EXPECT_EQ(driver.connect_attempts, 1);
}

TEST_F(EthernetLifecycleTest, FailedStartWithRejectedCleanupRemainsBlocked) {
  driver.start_ok = false;
  driver.stop_ok = false;
  this->ethernet_.enable();
  EXPECT_EQ(driver.stops, 1);

  driver.start_ok = true;
  this->ethernet_.enable();
  this->tick_();
  EXPECT_EQ(driver.starts, 1);
  EXPECT_EQ(driver.stops, 1);
  EXPECT_TRUE(this->ethernet_.is_disabled());
  EXPECT_FALSE(this->ethernet_.is_driver_stopped());
}

TEST_F(EthernetLifecycleTest, StoppingWhileConnectingDoesNotFireDisconnectAgain) {
  this->start_connecting_();
  int disconnects = 0;
  LambdaAction<> disconnect{[&] { ++disconnects; }};
  Automation<> automation{this->ethernet_.get_disconnect_trigger()};
  automation.add_action(&disconnect);

  this->ethernet_.disable();
  this->ethernet_.enable();
  this->driver_stopped_();
  this->tick_();
  EXPECT_EQ(disconnects, 0);
  EXPECT_EQ(driver.starts, 2);

  this->driver_started_();
  this->tick_();
  this->got_ip_();
  this->lost_link_();
  EXPECT_EQ(disconnects, 1);

  this->ethernet_.disable();
  this->ethernet_.enable();
  this->driver_stopped_();
  this->tick_();
  EXPECT_EQ(disconnects, 1);
  EXPECT_EQ(driver.starts, 3);
}

}  // namespace testing
}  // namespace esphome::ethernet
#endif  // USE_HOST
