#include <gtest/gtest.h>
#include <map>
#include "esphome/components/tmcm1240/tmcm1240.h"
#include "esphome/core/application.h"
#include "../canopen/test_helpers.h"

namespace esphome::tmcm1240::testing {

class ProbeMotor : public TMCM1240 {
 public:
  using TMCM1240::TMCM1240;
  void ready() {
    this->heartbeat_seen_ = true;
    this->last_heartbeat_ms_ = millis();
    this->nmt_state_ = 5;
    this->set_heartbeat_timeout(600000);
    this->set_run_current(0.385f);
    this->set_standby_current(0.1925f);
    this->set_stallguard_threshold(8);
    this->set_stallguard_min_velocity(2800);
  }
  void tick() {
    LoopBlockingGuard guard(nullptr, nullptr, millis());
    this->last_request_ms_ -= 50;
    this->loop();
  }
};

class MotorTest : public ::testing::Test {
 public:
  canopen::testing::MockCANbus bus;
  canopen::testing::ProbeClient client{&bus};
  ProbeMotor motor{&client, 32};
  std::map<uint16_t, uint32_t> objects{{0x6041, 0x27}, {0x6064, 10000}, {0x606C, 0}};
  std::vector<cia402::DriveResult> results;
  size_t processed{0};
  void SetUp() override {
    motor.ready();
    motor.add_on_result_callback([this](cia402::DriveResult result) { results.push_back(result); });
  }
  void step() {
    motor.tick();
    client.loop();
    if (processed == bus.sent.size())
      return;
    const auto &frame = bus.sent[processed++];
    const uint16_t index = frame.data[1] | (frame.data[2] << 8);
    uint32_t value = encode_uint32(frame.data[7], frame.data[6], frame.data[5], frame.data[4]);
    const bool read = frame.data[0] == 0x40;
    if (!read)
      objects[index] = value;
    else
      value = objects[index];
    const uint8_t size =
        index == 0x6041 ? 2 : (index == 0x6061 || index == 0x2102 || index == 0x209E || index == 0x20A7 ? 1 : 4);
    const uint8_t command = read ? 0x43 | ((4 - size) << 2) : 0x60;
    client.on_frame_(
        0x5A0, false, false,
        {command, frame.data[1], frame.data[2], frame.data[3], static_cast<uint8_t>(value),
         static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)});
  }
  void run() {
    for (unsigned i = 0; i < 500 && motor.busy(); i++)
      step();
  }
};

TEST(TMCM1240Current, QuantizesSnapshotRmsTargets) {
  EXPECT_EQ(TMCM1240::current_register(0.385f), 32);
  EXPECT_EQ(TMCM1240::current_register(0.1925f), 16);
}

TEST_F(MotorTest, AppliesVolatileSettingsWithoutSavingOrEnabling) {
  ASSERT_TRUE(motor.apply_configuration());
  run();
  EXPECT_EQ(objects[0x2003], 32);
  EXPECT_EQ(objects[0x2004], 16);
  EXPECT_EQ(objects[0x20B0], 0);
  EXPECT_EQ(objects[0x20A7], 1);
  EXPECT_EQ(objects.count(0x1010), 0);
  EXPECT_EQ(objects.count(0x6040), 0);
}

}  // namespace esphome::tmcm1240::testing
