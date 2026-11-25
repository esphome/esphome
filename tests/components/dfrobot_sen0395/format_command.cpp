#include <gtest/gtest.h>
#include <string>
#include "esphome/components/dfrobot_sen0395/dfrobot_sen0395.h"

namespace esphome::dfrobot_sen0395::testing {

// The text each command sends, as the per-command classes sent it before the queue held plain values.
class FormatCommandTest : public ::testing::Test {
 protected:
  std::string format_(CommandType type, uint8_t arg = 0) {
    char buf[64];
    this->radar_.format_command({type, arg}, buf, sizeof(buf));
    return buf;
  }

  DfrobotSen0395Component radar_;
};

TEST_F(FormatCommandTest, FixedCommands) {
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_POWER, true), "sensorStart");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_POWER, false), "sensorStop");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_FACTORY_RESET),
            "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SAVE_CFG), "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_RESET_SYSTEM), "resetSystem");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_LED_MODE, true), "setLedMode 1 0");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_LED_MODE, false), "setLedMode 1 1");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_UART_OUTPUT, true), "setUartOutput 1 1");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_UART_OUTPUT, false), "setUartOutput 1 0");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SENSOR_CFG_START, true), "sensorCfgStart 1");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SENSOR_CFG_START, false), "sensorCfgStart 0");
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SENSITIVITY, 7), "setSensitivity 7");
}

TEST_F(FormatCommandTest, DetectionRangeSegments) {
  this->radar_.enqueue_det_range_cfg(0.3f, 1.2f, -1, -1, -1, -1, -1, -1);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_DET_RANGE_CFG), "detRangeCfg -1 2 8");
  this->radar_.enqueue_det_range_cfg(0.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 9.0f);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_DET_RANGE_CFG), "detRangeCfg -1 0 10 13 20 27 33 40 60");
  // Segments stop at the first one with a negative bound
  this->radar_.enqueue_det_range_cfg(0.0f, 1.5f, 2.0f, -1, 4.0f, 5.0f, 6.0f, 9.0f);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_DET_RANGE_CFG), "detRangeCfg -1 0 10");
}

TEST_F(FormatCommandTest, DetectionRangeRoundsHalfStepsUp) {
  // 0.075 m is half a 0.15 m step
  this->radar_.enqueue_det_range_cfg(0.0f, 0.075f, -1, -1, -1, -1, -1, -1);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_DET_RANGE_CFG), "detRangeCfg -1 0 1");
}

TEST_F(FormatCommandTest, DetectionRangeNegativeFirstSegmentFallsBack) {
  this->radar_.enqueue_det_range_cfg(-1, 3.0f, -1, -1, -1, -1, -1, -1);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_DET_RANGE_CFG), "detRangeCfg -1 0 0");
}

TEST_F(FormatCommandTest, Latency) {
  this->radar_.enqueue_set_latency(0.0f, 2.5f);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SET_LATENCY), "setLatency 0.000 2.500");
  this->radar_.enqueue_set_latency(0.0125f, 0.0374f);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SET_LATENCY), "setLatency 0.025 0.025");
  this->radar_.enqueue_set_latency(1638.375f, 5000.0f);
  EXPECT_EQ(this->format_(CommandType::COMMAND_TYPE_SET_LATENCY), "setLatency 1638.375 1638.375");
}

}  // namespace esphome::dfrobot_sen0395::testing
