#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/emontx/emontx.h"
#include "common.h"

namespace esphome::emontx::testing {

namespace {
class EmonTxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.add_on_data_callback([this](StringRef line) { this->lines_.emplace_back(line.c_str(), line.size()); });
    this->sut_.add_on_json_callback([this](JsonObject, StringRef) { this->json_count_++; });
  }

  MockUARTComponent uart_;
  EmonTx sut_;
  std::vector<std::string> lines_;
  int json_count_{0};
};
}  // namespace

TEST_F(EmonTxTest, LinePassedToDataCallbacks) {
  this->uart_.push_rx("MSG:1\r\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:1");
  EXPECT_EQ(this->json_count_, 0);
}

TEST_F(EmonTxTest, JsonLineParsed) {
  this->uart_.push_rx("{\"P1\":100}\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->json_count_, 1);
}

TEST_F(EmonTxTest, OverflowedLineDiscardedUntilLineFeed) {
  // One byte more than the buffer holds, then more of the same line.
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->uart_.push_rx("tail\n");
  this->uart_.push_rx("MSG:2\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:2");
}

TEST_F(EmonTxTest, OverflowedLineTailNotParsedAsJson) {
  // The tail after the overflow looks like a JSON line and must not be parsed.
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->uart_.push_rx("{\"P1\":100}\n");
  this->sut_.loop();

  EXPECT_TRUE(this->lines_.empty());
  EXPECT_EQ(this->json_count_, 0);
}

TEST_F(EmonTxTest, OverflowAcrossLoopCalls) {
  this->uart_.push_rx(MAX_LINE_LENGTH + 1, 'a');
  this->sut_.loop();
  this->uart_.push_rx("tail\nMSG:3\n");
  this->sut_.loop();

  ASSERT_EQ(this->lines_.size(), 1u);
  EXPECT_EQ(this->lines_[0], "MSG:3");
}

}  // namespace esphome::emontx::testing
