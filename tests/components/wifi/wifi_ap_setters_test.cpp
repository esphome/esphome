#include <gtest/gtest.h>

#include <string>

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

TEST(WiFiAP, RuntimeSetters) {
  // Credentials from improv, captive portal, wifi.configure and lambdas arrive in RAM and are copied.
  WiFiAP ap;
  std::string ssid = "runtime_ssid";
  ap.set_ssid(ssid);
  char password[] = "runtime_pass";
  ap.set_password(password);
  ssid[0] = 'X';
  password[0] = 'X';
  EXPECT_EQ(ap.get_ssid(), "runtime_ssid");
  EXPECT_EQ(ap.get_password(), "runtime_pass");

  const char *ssid_ptr = ssid.c_str();
  ap.set_ssid(ssid_ptr);
  ap.set_password(StringRef(password, 4));
  EXPECT_EQ(ap.get_ssid(), "Xuntime_ssid");
  EXPECT_EQ(ap.get_password(), "Xunt");
}

}  // namespace esphome::wifi::testing
