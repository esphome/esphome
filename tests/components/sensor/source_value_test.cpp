#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

TEST(SourceValue, NullSensorIsNan) { EXPECT_TRUE(std::isnan(source_value(nullptr, -40.0f, 85.0f))); }

TEST(SourceValue, NoStateYetIsNan) {
  Sensor source;
  EXPECT_TRUE(std::isnan(source_value(&source, -40.0f, 85.0f)));
}

TEST(SourceValue, NanStateIsNan) {
  Sensor source;
  source.publish_state(NAN);
  EXPECT_TRUE(std::isnan(source_value(&source, -40.0f, 85.0f)));
}

TEST(SourceValue, InRangeStateIsReturned) {
  Sensor source;
  source.publish_state(21.5f);
  EXPECT_EQ(source_value(&source, -40.0f, 85.0f), 21.5f);
}

TEST(SourceValue, BoundsAreIncluded) {
  Sensor source;
  source.publish_state(-40.0f);
  EXPECT_EQ(source_value(&source, -40.0f, 85.0f), -40.0f);
  source.publish_state(85.0f);
  EXPECT_EQ(source_value(&source, -40.0f, 85.0f), 85.0f);
}

TEST(SourceValue, OutOfRangeStateIsNan) {
  Sensor source;
  source.publish_state(-40.1f);
  EXPECT_TRUE(std::isnan(source_value(&source, -40.0f, 85.0f)));
  source.publish_state(85.1f);
  EXPECT_TRUE(std::isnan(source_value(&source, -40.0f, 85.0f)));
  source.publish_state(INFINITY);
  EXPECT_TRUE(std::isnan(source_value(&source, -40.0f, 85.0f)));
}

TEST(SourceValue, LatestStateIsRead) {
  Sensor source;
  source.publish_state(101325.0f);
  EXPECT_TRUE(std::isnan(source_value(&source, 700.0f, 1200.0f)));
  source.publish_state(1013.0f);
  EXPECT_EQ(source_value(&source, 700.0f, 1200.0f), 1013.0f);
}

}  // namespace esphome::sensor::testing
