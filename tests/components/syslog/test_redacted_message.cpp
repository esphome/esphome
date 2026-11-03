#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/syslog/esphome_syslog.h"
#include "esphome/core/log.h"

namespace esphome::syslog::testing {

static std::string redact(const char *message) {
  char buf[128];
  size_t pos = append_redacted_message(buf, sizeof(buf), 0, message, strlen(message));
  return std::string(buf, pos);
}

TEST(SyslogRedactedMessage, PlainMessageUnchanged) {
  EXPECT_EQ(redact("WiFi connected to 'home'"), "WiFi connected to 'home'");
}

TEST(SyslogRedactedMessage, SecretReplaced) {
  EXPECT_EQ(redact("Password=" LOG_SECRET("'hunter2'") " ok"), "Password=[redacted] ok");
}

TEST(SyslogRedactedMessage, MultipleSecrets) {
  EXPECT_EQ(redact("SSID=" LOG_SECRET("home") " PSK=" LOG_SECRET("hunter2")), "SSID=[redacted] PSK=[redacted]");
}

TEST(SyslogRedactedMessage, UnterminatedSecretDropsRest) {
  EXPECT_EQ(redact("key=" ESPHOME_LOG_SECRET_BEGIN "abc"), "key=[redacted]");
}

TEST(SyslogRedactedMessage, AppendsAtOffsetAndCaps) {
  char buf[16] = "pre:";
  size_t pos = append_redacted_message(buf, sizeof(buf), 4, "0123456789abcdef", 16);
  EXPECT_EQ(pos, sizeof(buf) - 1);
  EXPECT_EQ(std::string(buf, pos), "pre:0123456789a");
  EXPECT_EQ(buf[pos], '\0');
}

}  // namespace esphome::syslog::testing
