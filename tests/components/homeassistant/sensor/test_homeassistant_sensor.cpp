#ifdef USE_HOST
#include <gtest/gtest.h>

#include <sys/socket.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/homeassistant/sensor/homeassistant_sensor.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/socket/socket.h"

namespace esphome::homeassistant::testing {

class HomeassistantSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // APIServer() makes itself the global_api_server that setup() subscribes to
    this->server_ = std::make_unique<api::APIServer>();
    // The handler only reads the server's subscriptions, so the connection is never started
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    this->connection_ = std::make_unique<api::APIConnection>(std::make_unique<socket::Socket>(fd), this->server_.get());
    this->sensor_.set_entity_id("sensor.power");
    this->sensor_.setup();
  }

  void TearDown() override {
    this->connection_.reset();
    this->server_.reset();
    api::global_api_server = nullptr;
  }

  // Deliver a state as Home Assistant sends it and return what was logged meanwhile. The connection writes a
  // terminator after the state, as it does in its receive buffer.
  std::string deliver_(const char *state) {
    std::string buffer = std::string(state) + "#";
    api::HomeAssistantStateResponse msg;
    msg.entity_id = StringRef("sensor.power");
    msg.state = StringRef(buffer.data(), strlen(state));
    const auto previous_baud_rate = logger::global_logger->get_baud_rate();
    ::testing::internal::CaptureStdout();
    logger::global_logger->set_baud_rate(115200);
    this->connection_->on_home_assistant_state_response(msg);
    logger::global_logger->set_baud_rate(previous_baud_rate);
    return ::testing::internal::GetCapturedStdout();
  }

  std::unique_ptr<api::APIServer> server_;
  std::unique_ptr<api::APIConnection> connection_;
  HomeassistantSensor sensor_;
};

TEST_F(HomeassistantSensorTest, NumberIsPublished) {
  this->deliver_("21.5");
  EXPECT_FLOAT_EQ(this->sensor_.state, 21.5f);
  this->deliver_("-3");
  EXPECT_FLOAT_EQ(this->sensor_.state, -3.0f);
}

TEST_F(HomeassistantSensorTest, NoValueIsPublishedAsNanWithoutAWarning) {
  for (const char *state : {"unavailable", "unknown", "None"}) {
    this->deliver_("21.5");
    const std::string logged = this->deliver_(state);
    EXPECT_TRUE(std::isnan(this->sensor_.state)) << state;
    EXPECT_EQ(logged.find("Can't convert"), std::string::npos) << state << " logged: " << logged;
  }
}

TEST_F(HomeassistantSensorTest, OtherTextIsPublishedAsNanWithAWarning) {
  this->deliver_("21.5");
  const std::string logged = this->deliver_("on");
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_NE(logged.find("Can't convert 'on' to number"), std::string::npos) << "logged: " << logged;
}

}  // namespace esphome::homeassistant::testing
#endif  // USE_HOST
