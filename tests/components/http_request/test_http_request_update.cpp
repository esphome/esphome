#include <gtest/gtest.h>

#include <string>

#include "esphome/components/http_request/update/http_request_update.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::http_request::testing {

namespace {

constexpr char LITERAL[] = "http://example.invalid/manifest.json";

}  // namespace

// Friend of HttpRequestUpdate; the host has no update task, so the check runs inline and
// check_running_ marks that window.
class HttpRequestUpdateTest : public ::testing::Test {
 protected:
  static void set_check_running(HttpRequestUpdate &update, bool running) { update.check_running_ = running; }
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
  update.set_source_url(update.get_source_url());
  EXPECT_STREQ(update.get_source_url(), "http://example.invalid/current-manifest.json");
}

// An automation run from inside the inline check (logger on_message) may replace the URL; the buffer
// the check started with must stay readable until the check is over.
TEST_F(HttpRequestUpdateTest, ReplacingDuringAnInlineCheckFreesTheOldUrlLater) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/first-manifest.json");
  const char *in_use = update.get_source_url();
  set_check_running(update, true);
  update.set_source_url("http://example.invalid/second-manifest.json");
  EXPECT_STREQ(in_use, "http://example.invalid/first-manifest.json");
  EXPECT_STREQ(update.get_source_url(), "http://example.invalid/second-manifest.json");
  set_check_running(update, false);
  // The deferred free runs on the next loop; a leak here would fail under the sanitizer.
  App.scheduler.call(millis());
  delay(1);
  App.scheduler.call(millis());
}

TEST(HttpRequestUpdateSourceUrl, ConfigLiteralAfterRuntimeUrlDropsTheCopy) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/runtime-manifest.json");
  update.set_source_url_static(LITERAL);
  EXPECT_EQ(update.get_source_url(), LITERAL);
}

}  // namespace esphome::http_request::testing
