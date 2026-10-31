#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/web_server_base/origin.h"

namespace esphome::web_server_base::testing {

static bool allowed(const char *origin, const char *host, const char *const *list) {
  return is_origin_allowed(origin, origin == nullptr ? 0 : strlen(origin), host, host == nullptr ? 0 : strlen(host),
                           list);
}

static constexpr const char *LIST[] = {"https://app.esphome.io", "http://other.local:8080", nullptr};
static constexpr const char *STAR[] = {"*", nullptr};

TEST(WebServerBaseOrigin, NoOriginIsAllowed) {
  EXPECT_TRUE(allowed(nullptr, nullptr, nullptr));
  EXPECT_TRUE(allowed("", "device.local", nullptr));
}

TEST(WebServerBaseOrigin, SameOriginIsAllowedWithoutList) {
  EXPECT_TRUE(allowed("http://device.local", "device.local", nullptr));
  EXPECT_TRUE(allowed("http://192.168.1.5:8080", "192.168.1.5:8080", nullptr));
  EXPECT_TRUE(allowed("https://device.example.com", "device.example.com", nullptr));
}

TEST(WebServerBaseOrigin, CrossOriginIsRejectedWithoutList) {
  EXPECT_FALSE(allowed("http://evil.example", "device.local", nullptr));
  // Port or prefix differences are a different origin
  EXPECT_FALSE(allowed("http://device.local:81", "device.local", nullptr));
  EXPECT_FALSE(allowed("http://device.local", "device.local:81", nullptr));
  EXPECT_FALSE(allowed("http://device.localx", "device.local", nullptr));
  EXPECT_FALSE(allowed("http://device.local", nullptr, nullptr));
  // Opaque origins such as sandboxed frames and file:// pages
  EXPECT_FALSE(allowed("null", "null", nullptr));
}

TEST(WebServerBaseOrigin, ListedOriginIsAllowed) {
  EXPECT_TRUE(allowed("https://app.esphome.io", "device.local", LIST));
  EXPECT_TRUE(allowed("http://other.local:8080", "device.local", LIST));
  EXPECT_TRUE(allowed("http://device.local", "device.local", LIST));
}

TEST(WebServerBaseOrigin, UnlistedOriginIsRejected) {
  EXPECT_FALSE(allowed("https://app.esphome.io.evil", "device.local", LIST));
  EXPECT_FALSE(allowed("https://app.esphome.i", "device.local", LIST));
  EXPECT_FALSE(allowed("http://other.local", "device.local", LIST));
  EXPECT_FALSE(allowed("null", "device.local", LIST));
}

TEST(WebServerBaseOrigin, StarEntryIsNotAWildcard) {
  // Codegen resolves "*" to USE_WEBSERVER_ALLOW_ANY_ORIGIN; the table itself never matches any origin
  EXPECT_FALSE(allowed("http://evil.example", "device.local", STAR));
  EXPECT_FALSE(allowed("null", "device.local", STAR));
}

TEST(WebServerBaseOrigin, SchemeSeparatorMustFollowFirstColon) {
  EXPECT_FALSE(allowed("http:", "device.local", nullptr));
  EXPECT_FALSE(allowed("http:/", "device.local", nullptr));
  EXPECT_FALSE(allowed("http:/device.local", "device.local", nullptr));
  EXPECT_FALSE(allowed("http:device.local", "device.local", nullptr));
}

TEST(WebServerBaseOrigin, LengthsAreNotNulTerminated) {
  // Arduino and ESP-IDF header values are passed with their length, not relied on for a terminator
  const char origin[] = "https://app.esphome.ioXXXX";
  EXPECT_TRUE(is_origin_allowed(origin, strlen("https://app.esphome.io"), "h", 1, LIST));
  const char host[] = "device.localXXXX";
  EXPECT_TRUE(
      is_origin_allowed("http://device.local", strlen("http://device.local"), host, strlen("device.local"), nullptr));
}

}  // namespace esphome::web_server_base::testing
