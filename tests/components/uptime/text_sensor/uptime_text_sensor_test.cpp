#include <gtest/gtest.h>

#include "../common.h"

namespace esphome::uptime::testing {

TEST(UptimeTextSensorTest, DefaultFormatStartsAtHighestNonZeroUnit) {
  auto sensor = make_default();
  EXPECT_EQ(publish(sensor, 1, 0), "0s");
  EXPECT_EQ(publish(sensor, 1, 59), "59s");
  EXPECT_EQ(publish(sensor, 1, MINUTE + 1), "1m1s");
  EXPECT_EQ(publish(sensor, 1, HOUR + 5), "1h0m5s");
  EXPECT_EQ(publish(sensor, 1, DAY), "1d0h0m0s");
  EXPECT_EQ(publish(sensor, 1, DAY + HOUR + MINUTE + 1), "1d1h1m1s");
  EXPECT_EQ(publish(sensor, 1, 400 * DAY + 23 * HOUR + 59 * MINUTE + 59), "400d23h59m59s");
}

TEST(UptimeTextSensorTest, CustomLabelsAndSeparator) {
  auto sensor = make_custom();
  EXPECT_EQ(publish(sensor, 1, 5), "5S");
  EXPECT_EQ(publish(sensor, 1, MINUTE + 1), "1M-1S");
  EXPECT_EQ(publish(sensor, 1, DAY + HOUR + MINUTE + 1), "1Days-1H-1M-1S");
}

TEST(UptimeTextSensorTest, ExpandShowsEveryEnabledUnit) {
  auto sensor = make_default(true);
  EXPECT_EQ(publish(sensor, 1, 5), "0d0h0m5s");
  EXPECT_EQ(publish(sensor, 1, HOUR), "0d1h0m0s");

  auto custom = make_custom(true);
  EXPECT_EQ(publish(custom, 1, 5), "0Days-0H-0M-5S");
  EXPECT_EQ(publish(custom, HOUR, 5), "0Days-0H");
}

TEST(UptimeTextSensorTest, SecondsDroppedFrom30sInterval) {
  auto sensor = make_default();
  EXPECT_EQ(publish(sensor, 29, DAY + HOUR + MINUTE + 1), "1d1h1m1s");
  EXPECT_EQ(publish(sensor, 30, DAY + HOUR + MINUTE + 1), "1d1h1m");
}

TEST(UptimeTextSensorTest, MinutesDroppedFrom30minInterval) {
  auto sensor = make_default();
  EXPECT_EQ(publish(sensor, 1799, DAY + HOUR + MINUTE + 1), "1d1h1m");
  EXPECT_EQ(publish(sensor, 1800, DAY + HOUR + MINUTE + 1), "1d1h");
}

TEST(UptimeTextSensorTest, HoursDroppedFrom12hInterval) {
  auto sensor = make_default();
  EXPECT_EQ(publish(sensor, 12 * HOUR - 1, DAY + HOUR + MINUTE + 1), "1d1h");
  EXPECT_EQ(publish(sensor, 12 * HOUR, DAY + HOUR + MINUTE + 1), "1d");
}

TEST(UptimeTextSensorTest, NeverIntervalShowsDaysOnly) {
  auto sensor = make_default();
  sensor.set_update_interval(SCHEDULER_DONT_RUN);
  sensor.publish_uptime(5);
  EXPECT_EQ(sensor.get_state(), "0d");
  sensor.publish_uptime(3 * DAY + HOUR + MINUTE + 1);
  EXPECT_EQ(sensor.get_state(), "3d");
}

TEST(UptimeTextSensorTest, ShowsSmallestEnabledUnitWhenAllShownUnitsAreZero) {
  auto sensor = make_default();
  EXPECT_EQ(publish(sensor, 30, 5), "0m");
  EXPECT_EQ(publish(sensor, 1800, 5), "0h");
  EXPECT_EQ(publish(sensor, 12 * HOUR, 5), "0d");
  EXPECT_EQ(publish(sensor, 30, HOUR + 5), "1h0m");
}

}  // namespace esphome::uptime::testing
