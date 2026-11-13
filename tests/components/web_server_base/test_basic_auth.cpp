#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "esphome/components/web_server_base/basic_auth.h"

namespace esphome::web_server_base::testing {

// base64("admin:secret")
static constexpr const char *HASH = "YWRtaW46c2VjcmV0";

static bool matches(const std::string &credentials, const char *expected = HASH) {
  return basic_auth_matches(credentials.data(), credentials.size(), expected, strlen(expected));
}

TEST(WebServerBaseBasicAuth, AcceptsTheHash) { EXPECT_TRUE(matches(HASH)); }

TEST(WebServerBaseBasicAuth, RejectsAWrongHash) {
  EXPECT_FALSE(matches("YWRtaW46c2VjcmVu"));  // last character differs
  EXPECT_FALSE(matches("ZWRtaW46c2VjcmV0"));  // first character differs
}

TEST(WebServerBaseBasicAuth, Base64IsCaseSensitive) { EXPECT_FALSE(matches("ywrtaw46c2vjcmv0")); }

TEST(WebServerBaseBasicAuth, RejectsOtherLengths) {
  EXPECT_FALSE(matches("YWRtaW46c2VjcmV"));
  EXPECT_FALSE(matches("YWRtaW46c2VjcmV0A"));
  EXPECT_FALSE(matches("YWRtaW46c2VjcmV0 extra"));
}

TEST(WebServerBaseBasicAuth, RejectsEmptyCredentials) {
  EXPECT_FALSE(matches(""));
  EXPECT_FALSE(matches("   "));
}

TEST(WebServerBaseBasicAuth, NothingMatchesAnEmptyHash) {
  EXPECT_FALSE(matches("", ""));
  EXPECT_FALSE(matches("x", ""));
}

// RFC 9110: credentials = auth-scheme [ 1*SP token68 ]; the library splits at the first space
TEST(WebServerBaseBasicAuth, SkipsExtraSpacesAfterTheScheme) { EXPECT_TRUE(matches(std::string("  ") + HASH)); }

TEST(WebServerBaseBasicAuth, LeadingTabIsNotASeparator) { EXPECT_FALSE(matches(std::string("\t") + HASH)); }

TEST(WebServerBaseBasicAuth, IgnoresTrailingWhitespace) {
  EXPECT_TRUE(matches(std::string(HASH) + " "));
  EXPECT_TRUE(matches(std::string(HASH) + " \t"));
}

TEST(WebServerBaseBasicAuth, LongCredentials) {
  // base64 of a 60-byte "user:password": longer than the 72-character line libb64 wraps at (#18237)
  const std::string hash = "dXNlcm5hbWVfdGhhdF9pc19sb25nOnBhc3N3b3JkX3RoYXRfaXNfZXZlbl9sb25nZXJfdGhhbl90aGF0";
  ASSERT_GT(hash.size(), 72u);
  EXPECT_TRUE(matches(hash, hash.c_str()));
  std::string wrong = hash;
  wrong[wrong.size() - 1] = 'A';
  EXPECT_FALSE(matches(wrong, hash.c_str()));
}

TEST(WebServerBaseBasicAuth, EmbeddedNulDoesNotEndTheCompare) {
  std::string with_nul(HASH);
  with_nul[3] = '\0';
  EXPECT_FALSE(matches(with_nul));
}

}  // namespace esphome::web_server_base::testing
