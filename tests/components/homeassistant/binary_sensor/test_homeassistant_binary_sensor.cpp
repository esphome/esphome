#ifdef USE_HOST
#include <gtest/gtest.h>

#include <sys/socket.h>

#include <cstring>
#include <memory>
#include <string>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/homeassistant/binary_sensor/homeassistant_binary_sensor.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/socket/socket.h"

namespace esphome::homeassistant::testing {

class HomeassistantBinarySensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // APIServer() makes itself the global_api_server that setup() subscribes to
    this->server_ = std::make_unique<api::APIServer>();
    // The handler only reads the server's subscriptions, so the connection is never started
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    this->connection_ = std::make_unique<api::APIConnection>(std::make_unique<socket::Socket>(fd), this->server_.get());
    this->binary_sensor_.set_entity_id("light.bedroom_light");
    this->binary_sensor_.setup();
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
    msg.entity_id = StringRef("light.bedroom_light");
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
  HomeassistantBinarySensor binary_sensor_;
};

TEST_F(HomeassistantBinarySensorTest, OnAndOffArePublished) {
  this->deliver_("on");
  EXPECT_TRUE(this->binary_sensor_.state);
  this->deliver_("off");
  EXPECT_FALSE(this->binary_sensor_.state);
}

TEST_F(HomeassistantBinarySensorTest, NoValueDoesNotChangeStateAndDoesNotWarn) {
  this->deliver_("on");
  for (const char *state : {"unavailable", "unknown", "None"}) {
    const std::string logged = this->deliver_(state);
    EXPECT_TRUE(this->binary_sensor_.has_state()) << state;
    EXPECT_TRUE(this->binary_sensor_.state) << state;
    EXPECT_EQ(logged.find("Can't convert"), std::string::npos) << state << " logged: " << logged;
  }
}

TEST_F(HomeassistantBinarySensorTest, OtherTextWarns) {
  this->deliver_("on");
  const std::string logged = this->deliver_("21.5");
  EXPECT_TRUE(this->binary_sensor_.state);
  EXPECT_NE(logged.find("Can't convert '21.5' to binary state"), std::string::npos) << "logged: " << logged;
}

}  // namespace esphome::homeassistant::testing
#endif  // USE_HOST
