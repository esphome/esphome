#include <gtest/gtest.h>

#include <string>

#include "esphome/components/http_request/update/http_request_update.h"

namespace esphome::http_request::testing {

namespace {

constexpr char LITERAL[] = "http://example.invalid/manifest.json";

}  // namespace

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

TEST(HttpRequestUpdateSourceUrl, ConfigLiteralAfterRuntimeUrlDropsTheCopy) {
  HttpRequestUpdate update;
  update.set_source_url("http://example.invalid/runtime-manifest.json");
  update.set_source_url_static(LITERAL);
  EXPECT_EQ(update.get_source_url(), LITERAL);
}

}  // namespace esphome::http_request::testing
