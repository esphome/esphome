#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/text_sensor/filter.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::text_sensor::testing {

static bool bracket_or_drop(std::string &x) {
  if (x.empty())
    return false;
  x = "[" + x + "]";
  return true;
}

TEST(TextSensorApplyFilter, PublishesTheModifiedValue) {
  TextSensor sensor;
  ApplyFilter<bracket_or_drop> filter;
  sensor.add_filter(&filter);
  std::vector<std::string> seen;
  sensor.add_on_state_callback([&seen](const std::string &state) { seen.push_back(state); });

  sensor.publish_state("a");
  sensor.publish_state("");
  sensor.publish_state("b");

  EXPECT_EQ(seen, (std::vector<std::string>{"[a]", "[b]"}));
  EXPECT_EQ(sensor.get_raw_state(), "b");
}

}  // namespace esphome::text_sensor::testing
