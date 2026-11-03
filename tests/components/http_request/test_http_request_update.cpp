#include <gtest/gtest.h>

#include <string>

#include "esphome/components/http_request/update/http_request_update.h"

#if defined(__SANITIZE_ADDRESS__)
#define HAS_ASAN
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HAS_ASAN
#endif
#endif
#ifdef HAS_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace esphome::http_request::testing {

namespace {

constexpr char LITERAL[] = "http://example.invalid/manifest.json";

// Freed heap memory is poisoned under AddressSanitizer, which CI builds with; without it the
// freed-exactly-once checks cannot be observed and are skipped.
void expect_freed(const char *buffer) {
#ifdef HAS_ASAN
  EXPECT_TRUE(__asan_address_is_poisoned(buffer));
#endif
}

void expect_live(const char *buffer, const char *content) {
#ifdef HAS_ASAN
  EXPECT_FALSE(__asan_address_is_poisoned(buffer));
#endif
  EXPECT_STREQ(buffer, content);
}

}  // namespace

// Friend of HttpRequestUpdate, so a check can be begun and ended without an HTTP fetch.
class HttpRequestUpdateTest : public ::testing::Test {
 protected:
  static void begin_check(HttpRequestUpdate &update, TaskResult &result) { update.begin_check_(&result); }
  static void end_check(HttpRequestUpdate &update, TaskResult &result) { update.end_check_(&result); }
};

TEST(HttpRequestUpdateSourceUrl, ConfigLiteralIsKeptByPointer) {
  HttpRequestUpdate update;
  update.set_source_url_static(LITERAL);
  EXPECT_EQ(update.get_source_url(), LITERAL);
}

TEST(HttpRequestUpdateSourceUrl, RuntimeStringIsCopied) {
  HttpRequestUpdate update;
  std::string url = "http://example.invalid/from-string-manifest.json";
  update.set_source_url(url);
  EXPECT_NE(update.get_source_url(), url.c_str());
  EXPECT_STREQ(update.get_source_url(), url.c_str());
}

// Device firmware passes base_url.c_str() of a local and runs the update after the lambda returned.
TEST(HttpRequestUpdateSourceUrl, PointerIntoDeadLocalStaysReadable) {
  HttpRequestUpdate update;
  {
    std::string local = "http://example.invalid/from-local-manifest.json";
    update.set_source_url(local.c_str());
  }
  EXPECT_STREQ(update.get_source_url(), "http://example.invalid/from-local-manifest.json");
}

TEST(HttpRequestUpdateSourceUrl, PassingTheCurrentUrlBackIsSafe) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/current-manifest.json");
  const char *previous = update.get_source_url();
  update.set_source_url(update.get_source_url());
  EXPECT_STREQ(update.get_source_url(), "http://example.invalid/current-manifest.json");
  expect_freed(previous);
}

TEST(HttpRequestUpdateSourceUrl, ConfigLiteralAfterRuntimeUrlDropsTheCopy) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/runtime-manifest.json");
  const char *copy = update.get_source_url();
  update.set_source_url_static(LITERAL);
  EXPECT_EQ(update.get_source_url(), LITERAL);
  expect_freed(copy);
}

// The URL a check started with stays readable until the check ends, however often it is replaced in
// the meantime; the replacements it never saw are freed at once and the original exactly once.
TEST_F(HttpRequestUpdateTest, UrlReplacedDuringACheckOutlivesIt) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/first-manifest.json");
  const char *first = update.get_source_url();
  TaskResult result;
  begin_check(update, result);
  EXPECT_EQ(result.source_url, first);

  update.set_source_url("http://example.invalid/second-manifest.json");
  const char *second = update.get_source_url();
  expect_live(first, "http://example.invalid/first-manifest.json");

  update.set_source_url("http://example.invalid/third-manifest.json");
  const char *third = update.get_source_url();
  expect_live(first, "http://example.invalid/first-manifest.json");
  expect_freed(second);

  end_check(update, result);
  expect_freed(first);
  expect_live(third, "http://example.invalid/third-manifest.json");
  // The entity owns the URL set during the check: switching away frees it.
  update.set_source_url_static(LITERAL);
  expect_freed(third);
}

TEST_F(HttpRequestUpdateTest, UrlUnchangedDuringACheckReturnsToTheEntity) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/kept-manifest.json");
  const char *kept = update.get_source_url();
  TaskResult result;
  begin_check(update, result);
  end_check(update, result);
  expect_live(kept, "http://example.invalid/kept-manifest.json");
  // Ownership came back, so the next change frees it instead of leaking it.
  update.set_source_url_static(LITERAL);
  expect_freed(kept);
}

TEST_F(HttpRequestUpdateTest, ConfigLiteralDuringACheckIsNeverFreed) {
  HttpRequestUpdate update;
  update.set_source_url_static(LITERAL);
  TaskResult result;
  begin_check(update, result);
  update.set_source_url("http://example.invalid/runtime-manifest.json");
  end_check(update, result);
  EXPECT_STREQ(LITERAL, "http://example.invalid/manifest.json");
  EXPECT_STREQ(update.get_source_url(), "http://example.invalid/runtime-manifest.json");
}

}  // namespace esphome::http_request::testing
