#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/cse7766/cse7766.h"
#include "esphome/components/sensor/sensor.h"
#include "../common.h"

namespace esphome::cse7766::testing {

namespace {

// Coefficients and cycles taken from a Sonoff S60 (CSE7759B). With this power coefficient a power
// cycle of 2507613 is about 2.16 W, and the cycle register advances by about 1050000 per second.
constexpr uint32_t VOLTAGE_COEFF = 0x030BB0;
constexpr uint32_t VOLTAGE_CYCLE = 0x000352;
constexpr uint32_t CURRENT_COEFF = 0x004137;
constexpr uint32_t CURRENT_CYCLE = 0x053339;
constexpr uint32_t POWER_COEFF = 5425000;
constexpr uint32_t LOW_LOAD_CYCLE = 2507613;
constexpr uint32_t SHORT_CYCLE = 900000;  // about 6 W, just under 1 s
constexpr uint32_t HIGH_LOAD_CYCLE = 2000;

constexpr uint8_t HEADER_OK = 0x55;
constexpr uint8_t HEADER_POWER_CYCLE_EXCEEDS_RANGE = 0xF2;
constexpr uint8_t ADJ_NEW_POWER = 0x71;
constexpr uint8_t ADJ_NO_NEW_POWER = 0x61;

void put24(std::vector<uint8_t> &frame, uint32_t value) {
  frame.push_back(value >> 16);
  frame.push_back(value >> 8);
  frame.push_back(value);
}

std::vector<uint8_t> make_frame(uint8_t header, uint32_t power_cycle, uint8_t adj) {
  std::vector<uint8_t> frame{header, 0x5A};
  put24(frame, VOLTAGE_COEFF);
  put24(frame, VOLTAGE_CYCLE);
  put24(frame, CURRENT_COEFF);
  put24(frame, CURRENT_CYCLE);
  put24(frame, POWER_COEFF);
  put24(frame, power_cycle);
  frame.push_back(adj);
  frame.push_back(0x0B);
  frame.push_back(0x81);
  uint8_t checksum = 0;
  for (size_t i = 2; i < frame.size(); i++) {
    checksum += frame[i];
  }
  frame.push_back(checksum);
  return frame;
}

class CSE7766Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_power_sensor(&this->power_);
  }

  float feed_(uint8_t header, uint32_t power_cycle, uint8_t adj) {
    this->uart_.push_rx(make_frame(header, power_cycle, adj));
    this->sut_.loop();
    return this->power_.state;
  }

  MockUARTComponent uart_;
  CSE7766Component sut_;
  sensor::Sensor power_;
};

}  // namespace

TEST_F(CSE7766Test, NewMeasurementIsReported) {
  EXPECT_NEAR(this->feed_(HEADER_OK, LOW_LOAD_CYCLE, ADJ_NEW_POWER), 2.163f, 0.001f);
}

// At low load one power cycle spans many frames. Frames in between carry the last cycle or the
// time since the last pulse, and must not be reported as 0 W.
TEST_F(CSE7766Test, LowLoadKeepsLastMeasurementBetweenPulses) {
  this->feed_(HEADER_OK, LOW_LOAD_CYCLE, ADJ_NEW_POWER);
  EXPECT_NEAR(this->feed_(HEADER_OK, LOW_LOAD_CYCLE, ADJ_NO_NEW_POWER), 2.163f, 0.001f);
  EXPECT_NEAR(this->feed_(HEADER_OK, 1078140, ADJ_NO_NEW_POWER), 2.163f, 0.001f);
  EXPECT_NEAR(this->feed_(HEADER_OK, 4900000, ADJ_NO_NEW_POWER), 2.163f, 0.001f);
}

// Below 1 s the chip does not send waiting times in normal operation, so such a frame
// means the pulse is overdue and 0 W is reported at once, as before
TEST_F(CSE7766Test, ShortCycleReportsZeroWithoutNewMeasurement) {
  this->feed_(HEADER_OK, SHORT_CYCLE, ADJ_NEW_POWER);
  EXPECT_EQ(this->feed_(HEADER_OK, 1050000, ADJ_NO_NEW_POWER), 0.0f);
}

TEST_F(CSE7766Test, OverduePulseReportsZero) {
  this->feed_(HEADER_OK, LOW_LOAD_CYCLE, ADJ_NEW_POWER);
  EXPECT_EQ(this->feed_(HEADER_OK, 2 * LOW_LOAD_CYCLE + 1, ADJ_NO_NEW_POWER), 0.0f);
  // Stays 0 even when a later frame shows a short cycle again, until a new measurement arrives
  EXPECT_EQ(this->feed_(HEADER_OK, 1000000, ADJ_NO_NEW_POWER), 0.0f);
}

// A large load switching off: the first frame without a new measurement already shows about one
// second since the last pulse, far beyond the 2 ms cycle, so 0 W is reported at once.
TEST_F(CSE7766Test, HighLoadSwitchingOffReportsZeroAtOnce) {
  EXPECT_NEAR(this->feed_(HEADER_OK, HIGH_LOAD_CYCLE, ADJ_NEW_POWER), 2712.5f, 0.1f);
  EXPECT_EQ(this->feed_(HEADER_OK, 1072601, ADJ_NO_NEW_POWER), 0.0f);
}

TEST_F(CSE7766Test, NoMeasurementYetReportsZero) { EXPECT_EQ(this->feed_(HEADER_OK, 1078140, ADJ_NO_NEW_POWER), 0.0f); }

TEST_F(CSE7766Test, PowerCycleExceedsRangeReportsZero) {
  this->feed_(HEADER_OK, LOW_LOAD_CYCLE, ADJ_NEW_POWER);
  EXPECT_EQ(this->feed_(HEADER_POWER_CYCLE_EXCEEDS_RANGE, 9000000, ADJ_NO_NEW_POWER), 0.0f);
  EXPECT_EQ(this->feed_(HEADER_OK, 1078140, ADJ_NO_NEW_POWER), 0.0f);
}

}  // namespace esphome::cse7766::testing
