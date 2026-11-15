#include <gtest/gtest.h>

#include <string>

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

TEST(CompactString, EmptyFromNullptr) {
  CompactString str(nullptr, 0);
  EXPECT_TRUE(str.empty());
  EXPECT_STREQ(str.c_str(), "");
}

TEST(CompactString, InlineUpToCapacity) {
  const std::string text(CompactString::INLINE_CAPACITY, 'a');
  CompactString str(text.c_str(), text.size());
  EXPECT_EQ(str.size(), text.size());
  EXPECT_EQ(std::string(str.c_str()), text);
}

TEST(CompactString, HeapPastCapacity) {
  const std::string text(CompactString::INLINE_CAPACITY + 1, 'b');
  CompactString str(text.c_str(), text.size());
  EXPECT_EQ(str.size(), text.size());
  EXPECT_EQ(std::string(str.c_str()), text);

  CompactString copy(str);
  EXPECT_EQ(copy, str);
}

TEST(CompactString, ClampsToMaxLength) {
  const std::string text(CompactString::MAX_LENGTH + 10, 'c');
  CompactString str(text.c_str(), text.size());
  EXPECT_EQ(str.size(), CompactString::MAX_LENGTH);
  EXPECT_EQ(std::string(str.c_str()), text.substr(0, CompactString::MAX_LENGTH));
}

TEST(WiFiAP, FlashLiteralSetters) {
  // ESPHOME_F is what codegen emits; it keeps the text in flash on ESP8266.
  WiFiAP ap;
  ap.set_ssid(ESPHOME_F("network"));
  ap.set_password(ESPHOME_F("secret"));
  EXPECT_EQ(ap.get_ssid(), "network");
  EXPECT_EQ(ap.get_password(), "secret");
}

TEST(WiFiAP, RuntimeSetters) {
  // Credentials from improv, captive portal, wifi.configure and lambdas arrive in RAM.
  WiFiAP ap;
  std::string ssid = "runtime_ssid";
  ap.set_ssid(ssid);
  char password[] = "runtime_pass";
  ap.set_password(password);
  EXPECT_EQ(ap.get_ssid(), "runtime_ssid");
  EXPECT_EQ(ap.get_password(), "runtime_pass");

  // The setters copied the text, so changing the source afterwards leaves them alone.
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
