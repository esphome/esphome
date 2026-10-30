#include <gtest/gtest.h>

#include <cmath>
#include <deque>

#include "esphome/components/mhz19/mhz19.h"
#include "esphome/core/application.h"

namespace esphome::mhz19 {
uint8_t mhz19_checksum(const uint8_t *command);
}  // namespace esphome::mhz19

namespace esphome::mhz19::testing {
namespace {

// Answers every GET_PPM request with a valid 400 ppm, 25 °C frame.
class FakeUART final : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    if (len >= 3 && data[0] == 0xFF && data[2] == 0x86) {
      static constexpr uint8_t FRAME[] = {0xFF, 0x86, 0x01, 0x90, 25 + 40, 0x00, 0x00, 0x00};
      this->rx.assign(std::begin(FRAME), std::end(FRAME));
      this->rx.push_back(mhz19_checksum(FRAME));
    }
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    for (size_t i = 0; i < len; i++) {
      data[i] = this->rx.front();
      this->rx.pop_front();
    }
    return true;
  }
  bool peek_byte(uint8_t *data) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}

  std::deque<uint8_t> rx;
};

class MHZ19WarmupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->mhz19_.set_uart_parent(&this->uart_);
    this->mhz19_.set_co2_sensor(&this->co2_);
    this->mhz19_.set_warmup_seconds(75);
  }

  bool update_at_(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};  // sets the loop's cached time
    this->co2_.state = NAN;
    this->mhz19_.update();
    return !std::isnan(this->co2_.state);
  }

  FakeUART uart_;
  sensor::Sensor co2_;
  MHZ19Component mhz19_;
};

}  // namespace

TEST_F(MHZ19WarmupTest, SkipsReadingsDuringWarmup) {
  EXPECT_FALSE(this->update_at_(1000));
  EXPECT_FALSE(this->update_at_(74999));
  EXPECT_TRUE(this->update_at_(75000));
  EXPECT_FLOAT_EQ(this->co2_.state, 400.0f);
}

// About 49.7 days after boot the uptime counter wraps to 0; readings must not stop for another warm-up.
TEST_F(MHZ19WarmupTest, KeepsReadingAfterUptimeWraps) {
  EXPECT_TRUE(this->update_at_(4000000000U));
  EXPECT_TRUE(this->update_at_(500));
  EXPECT_TRUE(this->update_at_(74999));
  EXPECT_FLOAT_EQ(this->co2_.state, 400.0f);
}

}  // namespace esphome::mhz19::testing
