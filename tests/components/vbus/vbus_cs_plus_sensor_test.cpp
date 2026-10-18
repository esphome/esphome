#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "esphome/components/vbus/sensor/vbus_sensor.h"

namespace esphome::vbus::testing {
namespace {

class VBusCSPlusSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->decoder_.set_command(0x0100);
    this->decoder_.set_source(0x2211);
    this->decoder_.set_dest(0x0010);
    this->decoder_.set_temperature1_sensor(&this->sensors_[0]);
    this->decoder_.set_temperature2_sensor(&this->sensors_[1]);
    this->decoder_.set_temperature3_sensor(&this->sensors_[2]);
    this->decoder_.set_temperature4_sensor(&this->sensors_[3]);
    this->decoder_.set_temperature5_sensor(&this->sensors_[4]);
    this->decoder_.set_pump_speed1_sensor(&this->sensors_[5]);
    this->decoder_.set_pump_speed2_sensor(&this->sensors_[6]);
    this->decoder_.set_operating_hours1_sensor(&this->sensors_[7]);
    this->decoder_.set_operating_hours2_sensor(&this->sensors_[8]);
    this->decoder_.set_heat_quantity_sensor(&this->sensors_[9]);
    this->decoder_.set_time_sensor(&this->sensors_[10]);
    this->decoder_.set_version_sensor(&this->sensors_[11]);
    this->decoder_.set_flow_rate_sensor(&this->sensors_[12]);
    for (size_t i = 0; i < this->sensors_.size(); ++i)
      this->sensors_[i].add_on_state_callback([this, i](float) { ++this->publications_[i]; });
  }
  void feed_(std::vector<uint8_t> payload) { this->decoder_.on_message(0x0100, 0x2211, 0x0010, payload); }

  // Expected values were independently decoded with resol-vbus 0.28.0's CS Plus VSF.
  std::vector<uint8_t> reference_{0x85, 0xff, 0xa7, 0x01, 0xaa, 0x02, 0xcd, 0x00, 37,   0,    0xf0, 0x0e, 100, 0,
                                  0x77, 0x05, 0,    4,    0,    0,    0,    0,    0x0d, 0x03, 0,    0,    0,   0,
                                  0x1a, 0xd4, 0x51, 0,    111,  0,    0,    0,    0x19, 0xfc, 0x19, 0xfc};
  DeltaSolCSPlusSensor decoder_;
  std::array<sensor::Sensor, 13> sensors_;
  std::array<unsigned, 13> publications_{};
};

TEST_F(VBusCSPlusSensorTest, DecodesAllExistingFieldsIncludingSignedFlowAndEnergy) {
  this->feed_(this->reference_);
  const std::array<float, 13> expected{-12.3f, 42.3f, 68.2f,   20.5f, -99.9f, 37,  100,
                                       3824,   1399,  5362714, 781,   1.11f,  -999};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(this->sensors_[i].state, expected[i]) << i;
    EXPECT_EQ(this->publications_[i], 1u) << i;
  }
}

class VBusCSPlusTruncatedTest : public VBusCSPlusSensorTest, public ::testing::WithParamInterface<size_t> {};

TEST_P(VBusCSPlusTruncatedTest, TruncatedPayloadDoesNotPublishAnyPartialOrOutOfBoundsValue) {
  this->feed_(this->reference_);
  auto shortened = this->reference_;
  shortened.resize(GetParam());
  for (auto &byte : shortened)
    byte = 0;
  this->feed_(shortened);
  for (size_t i = 0; i < this->sensors_.size(); ++i)
    EXPECT_EQ(this->publications_[i], 1u) << i;
  EXPECT_FLOAT_EQ(this->sensors_[0].state, -12.3f);
  EXPECT_FLOAT_EQ(this->sensors_[12].state, -999);
}

TEST_P(VBusCSPlusTruncatedTest, FlowOnlyConfigurationRejectsEveryShortPayload) {
  DeltaSolCSPlusSensor decoder;
  sensor::Sensor flow;
  decoder.set_flow_rate_sensor(&flow);
  auto shortened = this->reference_;
  shortened.resize(GetParam());
  decoder.on_message(0x0100, 0x2211, 0x0010, shortened);
  EXPECT_FALSE(flow.has_state());
}

INSTANTIATE_TEST_SUITE_P(AllShortPayloads, VBusCSPlusTruncatedTest, ::testing::Range(size_t{0}, size_t{40}));

TEST_F(VBusCSPlusSensorTest, FlowDecodesTheEntireSignedSixteenBitRange) {
  for (int32_t value = -32768; value <= 32767; ++value) {
    const uint16_t raw = static_cast<uint16_t>(value);
    this->reference_[38] = raw & 0xff;
    this->reference_[39] = raw >> 8;
    this->feed_(this->reference_);
    ASSERT_FLOAT_EQ(this->sensors_[12].state, static_cast<float>(value)) << value;
  }
}

TEST_F(VBusCSPlusSensorTest, ExtraTrailingFieldsDoNotChangeExistingMeasurements) {
  this->reference_.resize(64, 0xff);
  this->feed_(this->reference_);
  EXPECT_FLOAT_EQ(this->sensors_[0].state, -12.3f);
  EXPECT_FLOAT_EQ(this->sensors_[9].state, 5362714);
  EXPECT_FLOAT_EQ(this->sensors_[12].state, -999);
}

TEST_F(VBusCSPlusSensorTest, UnconfiguredSensorsDoNotPreventDecoding) {
  DeltaSolCSPlusSensor decoder;
  sensor::Sensor flow;
  decoder.set_flow_rate_sensor(&flow);
  decoder.on_message(0x0100, 0x2211, 0x0010, this->reference_);
  EXPECT_FLOAT_EQ(flow.state, -999);
}

TEST_F(VBusCSPlusSensorTest, OtherControllersDestinationsAndCommandsRemainFiltered) {
  for (auto fields :
       {std::array<uint16_t, 3>{0x0100, 0x2212, 0x0010}, {0x0100, 0x2211, 0x0020}, {0x0200, 0x2211, 0x0010}})
    this->decoder_.on_message(fields[0], fields[1], fields[2], this->reference_);
  for (unsigned count : this->publications_)
    EXPECT_EQ(count, 0u);
}

}  // namespace
}  // namespace esphome::vbus::testing
