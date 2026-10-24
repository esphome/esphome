#include "../common.h"
#include "esphome/components/mitsubishi_cn105/sensor/mitsubishi_cn105_sensor.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105SensorTests, IgnoresReadingUntilASampleArrives) {
  MitsubishiCN105Component hub;
  MitsubishiCN105Sensor sensor;
  sensor.set_kind(MitsubishiCN105SensorKind::INPUT_POWER);
  sensor.set_parent(&hub);
  sensor.setup();

  hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  hub.set_target_temperature(20.0f);
  hub.publish_status();

  EXPECT_FALSE(sensor.has_state());
  EXPECT_THAT(hub.status().input_power, ::testing::IsNan());
}

}  // namespace esphome::mitsubishi_cn105::testing
