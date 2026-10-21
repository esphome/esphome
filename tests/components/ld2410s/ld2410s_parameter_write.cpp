#include <algorithm>
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include "esphome/components/ld2410s/ld2410s.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

#if !defined(USE_LD2410S_PARAMETERS) || !defined(USE_LD2410S_VERSION)
#error "tests/components/ld2410s/number/__init__.py must define the LD2410S features"
#endif

namespace esphome::ld2410s::testing {

// Vectors, so EXPECT_EQ prints both command lists on a mismatch
static const std::vector<uint16_t> INIT_COMMANDS = {0x00FF, 0x0000, 0x0071, 0x007A, 0x00FE};
static const std::vector<uint16_t> WRITE_COMMANDS = {0x00FF, 0x0070, 0x00FE};
static const std::vector<uint16_t> CONFIG_MODE_ON = {0x00FF};
static constexpr uint32_t STEP_MS = 300;
// 0x0070 frame: 4 header + 2 length + 2 command + 6 * (2 key + 4 value) + 4 footer
static constexpr size_t WRITE_FRAME_SIZE = 48;
static constexpr size_t MAX_DISTANCE_VALUE_POS = 10;
static constexpr size_t NO_DELAY_VALUE_POS = 22;
// Init far from the wrap, across it, and ending just before it: nine 300 ms passes put the last
// acknowledgement at 0xFFFFFF00, so the gap itself crosses the wrap
static constexpr uint32_t START_FAR_FROM_WRAP = 1000;
static constexpr uint32_t START_ACROSS_WRAP = 0xFFFFFC00;
static constexpr uint32_t START_GAP_ACROSS_WRAP = 0xFFFFF474;

// Acknowledges every command in the layout ld2410s.cpp parses; a fixture, not a hardware claim
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    const uint16_t command = static_cast<uint16_t>(data[6] | (data[7] << 8));
    this->commands.push_back(command);
    this->frames.emplace_back(data, data + len);
    std::vector<uint8_t> payload = {data[6], static_cast<uint8_t>(data[7] | 0x01), 0x00, 0x00};
    if (command == 0x0071)  // six 32-bit values, each within a byte
      payload.insert(payload.end(), {8, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 20, 0, 0, 0, 20, 0, 0, 0, 5, 0, 0, 0});
    if (command == 0x0000)  // device type and three version words
      payload.insert(payload.end(), size_t{10}, uint8_t{0});
    std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, static_cast<uint8_t>(payload.size()), 0x00};
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
    this->queue(frame);
  }
  void queue(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_.size() - this->pos_ < len)
      return false;
    std::copy_n(this->rx_.begin() + this->pos_, len, data);
    this->pos_ += len;
    return true;
  }
  size_t available() override { return this->rx_.size() - this->pos_; }
  bool peek_byte(uint8_t *data) override { return false; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

  std::vector<uint16_t> commands;
  std::vector<std::vector<uint8_t>> frames;

 protected:
  std::vector<uint8_t> rx_;
  size_t pos_{0};
};

class LD2410SWriteTest : public ::testing::Test {
 public:
  LD2410SWriteTest() { this->radar_.set_uart_parent(&this->uart_); }

 protected:
  void loop_at_(uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    this->radar_.loop();
  }
  // Runs init until the configuration-mode-off acknowledgement is taken, then queues one change
  void finish_init_(uint32_t now) {
    for (int pass = 0; pass < 20; pass++, now += STEP_MS) {
      this->loop_at_(now);
      if (!this->uart_.commands.empty() && this->uart_.commands.back() == 0x00FE && this->uart_.available() == 0)
        break;
    }
    ASSERT_EQ(this->uart_.commands, INIT_COMMANDS);
    ASSERT_EQ(this->uart_.available(), 0u);
    ASSERT_TRUE(this->radar_.set_parameter(Parameter::PARAMETER_NO_DELAY, 30));
    this->acked_ = now;
    this->sent_ = this->uart_.commands.size();
  }
  // Loops every 300 ms with a short data frame before each pass, so the link check never restarts init
  std::vector<uint16_t> write_from_(uint32_t now, int passes) {
    for (int pass = 0; pass < passes; pass++, now += STEP_MS) {
      this->uart_.queue({0x6E, 0x02, 0x64, 0x00, 0x62});
      this->loop_at_(now);
    }
    return {this->uart_.commands.begin() + this->sent_, this->uart_.commands.end()};
  }
  void check_waits_for_gap_(uint32_t start, uint32_t elapsed) {
    this->finish_init_(start);
    if (::testing::Test::HasFatalFailure())
      return;
    EXPECT_TRUE(this->write_from_(this->acked_ + elapsed, 1).empty());
    EXPECT_EQ(this->write_from_(this->acked_ + STEP_MS, 1), CONFIG_MODE_ON);
    EXPECT_EQ(this->write_from_(this->acked_ + 2 * STEP_MS, 5), WRITE_COMMANDS);
  }
  void check_starts_on_first_pass_(uint32_t start, uint32_t elapsed) {
    this->finish_init_(start);
    if (::testing::Test::HasFatalFailure())
      return;
    const uint32_t now = this->acked_ + elapsed;
    EXPECT_EQ(this->write_from_(now, 1), CONFIG_MODE_ON);
    // The write, not a re-init (which would send 0000 next)
    ASSERT_EQ(this->write_from_(now + STEP_MS, 5), WRITE_COMMANDS);
    const auto &frame = this->uart_.frames[this->sent_ + 1];
    ASSERT_EQ(frame.size(), WRITE_FRAME_SIZE);
    EXPECT_EQ(frame[NO_DELAY_VALUE_POS], 30);
    EXPECT_EQ(frame[MAX_DISTANCE_VALUE_POS], 8);  // the module's own value, read during init
  }

  FakeUART uart_;
  LD2410SComponent radar_;
  uint32_t acked_{0};
  size_t sent_{0};
};

// Time since the last acknowledgement is inside the 300 ms gap: nothing is sent until it ends
class LD2410SGapTest : public LD2410SWriteTest {};
// Time since the last acknowledgement is at or past the gap end, up to the full 32-bit range
class LD2410SStartTest : public LD2410SWriteTest {};

TEST_F(LD2410SGapTest, FarFromWrapElapsed0) { this->check_waits_for_gap_(START_FAR_FROM_WRAP, 0); }
TEST_F(LD2410SGapTest, FarFromWrapElapsed100) { this->check_waits_for_gap_(START_FAR_FROM_WRAP, 100); }
TEST_F(LD2410SGapTest, FarFromWrapElapsed299) { this->check_waits_for_gap_(START_FAR_FROM_WRAP, 299); }
TEST_F(LD2410SGapTest, AcrossWrapElapsed0) { this->check_waits_for_gap_(START_ACROSS_WRAP, 0); }
TEST_F(LD2410SGapTest, AcrossWrapElapsed100) { this->check_waits_for_gap_(START_ACROSS_WRAP, 100); }
TEST_F(LD2410SGapTest, AcrossWrapElapsed299) { this->check_waits_for_gap_(START_ACROSS_WRAP, 299); }
TEST_F(LD2410SGapTest, GapAcrossWrapElapsed0) { this->check_waits_for_gap_(START_GAP_ACROSS_WRAP, 0); }
TEST_F(LD2410SGapTest, GapAcrossWrapElapsed100) { this->check_waits_for_gap_(START_GAP_ACROSS_WRAP, 100); }
TEST_F(LD2410SGapTest, GapAcrossWrapElapsed299) { this->check_waits_for_gap_(START_GAP_ACROSS_WRAP, 299); }

TEST_F(LD2410SStartTest, FarFromWrapGapEnd) { this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 300); }
TEST_F(LD2410SStartTest, FarFromWrapGapEndPlus1) { this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 301); }
TEST_F(LD2410SStartTest, FarFromWrapHalfRangeMinus1) {
  this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 0x8000012B);
}
TEST_F(LD2410SStartTest, FarFromWrapHalfRange) { this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 0x8000012C); }
TEST_F(LD2410SStartTest, FarFromWrapThreeQuarterRange) {
  this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 0xC0000000);
}
TEST_F(LD2410SStartTest, FarFromWrapFullRange) { this->check_starts_on_first_pass_(START_FAR_FROM_WRAP, 0xFFFFFFFF); }
TEST_F(LD2410SStartTest, AcrossWrapGapEnd) { this->check_starts_on_first_pass_(START_ACROSS_WRAP, 300); }
TEST_F(LD2410SStartTest, AcrossWrapGapEndPlus1) { this->check_starts_on_first_pass_(START_ACROSS_WRAP, 301); }
TEST_F(LD2410SStartTest, AcrossWrapHalfRangeMinus1) {
  this->check_starts_on_first_pass_(START_ACROSS_WRAP, 0x8000012B);
}
TEST_F(LD2410SStartTest, AcrossWrapHalfRange) { this->check_starts_on_first_pass_(START_ACROSS_WRAP, 0x8000012C); }
TEST_F(LD2410SStartTest, AcrossWrapThreeQuarterRange) {
  this->check_starts_on_first_pass_(START_ACROSS_WRAP, 0xC0000000);
}
TEST_F(LD2410SStartTest, AcrossWrapFullRange) { this->check_starts_on_first_pass_(START_ACROSS_WRAP, 0xFFFFFFFF); }
TEST_F(LD2410SStartTest, GapAcrossWrapGapEnd) { this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 300); }
TEST_F(LD2410SStartTest, GapAcrossWrapGapEndPlus1) { this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 301); }
TEST_F(LD2410SStartTest, GapAcrossWrapHalfRangeMinus1) {
  this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 0x8000012B);
}
TEST_F(LD2410SStartTest, GapAcrossWrapHalfRange) {
  this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 0x8000012C);
}
TEST_F(LD2410SStartTest, GapAcrossWrapThreeQuarterRange) {
  this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 0xC0000000);
}
TEST_F(LD2410SStartTest, GapAcrossWrapFullRange) {
  this->check_starts_on_first_pass_(START_GAP_ACROSS_WRAP, 0xFFFFFFFF);
}

}  // namespace esphome::ld2410s::testing
