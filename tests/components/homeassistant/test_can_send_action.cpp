#ifdef USE_HOST
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "esphome/components/api/api_server.h"
#include "esphome/components/homeassistant/homeassistant.h"
#include "esphome/components/logger/logger.h"

#ifdef USE_API_HOMEASSISTANT_SERVICES
namespace esphome::homeassistant::testing {

TEST(HomeassistantCanSendActionTest, NoClientConnectedRefusesAndLogs) {
  // APIServer() makes itself the global_api_server; no connection is ever added
  auto server = std::make_unique<api::APIServer>();
  const auto previous_baud_rate = logger::global_logger->get_baud_rate();
  ::testing::internal::CaptureStdout();
  logger::global_logger->set_baud_rate(115200);
  const bool allowed = can_send_action("test", "switch.kitchen", nullptr);
  logger::global_logger->set_baud_rate(previous_baud_rate);
  const std::string logged = ::testing::internal::GetCapturedStdout();
  server.reset();
  api::global_api_server = nullptr;

  EXPECT_FALSE(allowed);
  EXPECT_NE(logged.find("'switch.kitchen': No clients connected to API server"), std::string::npos)
      << "logged: " << logged;
}

}  // namespace esphome::homeassistant::testing
#endif  // USE_API_HOMEASSISTANT_SERVICES
#endif  // USE_HOST
