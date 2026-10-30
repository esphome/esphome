#include <gtest/gtest.h>

#include <cmath>

#include "../common.h"
#include "esphome/components/scd4x/scd4x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::scd4x::testing {

static constexpr uint16_t AMBIENT_PRESSURE_COMMAND = 0xe000;

// One test: the scheduler outlives a fixture, so a second component would see the first one's timeouts
TEST(SCD4XPressureSource, OnlyInRangeReadingsAreRoundedAndWritten) {
  SensirionBus bus;
  sensor::Sensor pressure;
  SCD4XComponent scd;
  scd.set_i2c_bus(&bus);
  scd.set_i2c_address(0x62);
  scd.set_ambient_pressure_source(&pressure);
  scd.set_temperature_offset(4.0f);
  // setup() waits 1000 ms, then 500 ms, before the sensor is initialized
  scd.setup();
  for (uint32_t wait : {1010, 510}) {
    delay(wait);
    App.scheduler.call(millis());
  }

  auto update_with = [&](float value) {
    bus.writes.clear();
    pressure.publish_state(value);
    scd.update();
    return bus.words_for(AMBIENT_PRESSURE_COMMAND);
  };

  EXPECT_EQ(update_with(1013.6f), std::vector<uint16_t>{1014});
  EXPECT_TRUE(update_with(101325.0f).empty());
  EXPECT_TRUE(update_with(500.0f).empty());
  EXPECT_TRUE(update_with(NAN).empty());
  EXPECT_EQ(update_with(700.0f), std::vector<uint16_t>{700});
  EXPECT_EQ(update_with(1200.0f), std::vector<uint16_t>{1200});
  // The range check runs before rounding, so values just outside are rejected rather than rounded in
  EXPECT_TRUE(update_with(699.6f).empty());
  EXPECT_TRUE(update_with(1200.4f).empty());

  // A lambda calling the setter directly gets the same check
  auto set_directly = [&](float value) {
    bus.writes.clear();
    scd.set_ambient_pressure_compensation(value);
    return bus.words_for(AMBIENT_PRESSURE_COMMAND);
  };
  EXPECT_TRUE(set_directly(101325.0f).empty());
  EXPECT_TRUE(set_directly(NAN).empty());
  EXPECT_TRUE(set_directly(-5.0f).empty());
  EXPECT_EQ(set_directly(1013.0f), std::vector<uint16_t>{1013});

  // Run the reads each update() queued while `scd` is still alive
  bus.writes.clear();
  App.scheduler.call(millis());
  EXPECT_FALSE(bus.writes.empty());
}

}  // namespace esphome::scd4x::testing
