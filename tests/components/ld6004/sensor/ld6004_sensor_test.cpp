#ifdef USE_HOST
#include "../common.h"

#include "esphome/components/sensor/sensor.h"

namespace esphome::ld6004::testing {

namespace {

constexpr uint16_t TYPE_REPORT_DWELL_AREAS = 0x0A16;

}  // namespace

// A 0x0A16 report lands on the dwell areas only, not on the interference or detection ones
TEST_F(LD6004Test, DwellAreaReportPublishesDwellBounds) {
  sensor::Sensor dwell[ld600x::AREA_AXIS_COUNT];
  sensor::Sensor interference_x_min;
  sensor::Sensor detection_x_min;
  for (uint8_t axis = 0; axis < ld600x::AREA_AXIS_COUNT; axis++)
    this->sut_.set_area_sensor(ld600x::AREA_KIND_DWELL, 3, axis, &dwell[axis]);
  this->sut_.set_area_sensor(ld600x::AREA_KIND_INTERFERENCE, 3, ld600x::AREA_AXIS_X_MIN, &interference_x_min);
  this->sut_.set_area_sensor(ld600x::AREA_KIND_DETECTION, 3, ld600x::AREA_AXIS_X_MIN, &detection_x_min);

  // x_min, x_max, y_min, y_max, z_min, z_max for area 3; the other areas are zero
  const float bounds[ld600x::AREA_AXIS_COUNT] = {-1.5f, 1.25f, 0.5f, 3.75f, -0.25f, 2.5f};
  std::vector<uint8_t> p;
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    for (float v : bounds)
      put_f32(p, i == 3 ? v : 0.0f);
  }
  this->feed_(tf_frame(TYPE_REPORT_DWELL_AREAS, p));

  for (uint8_t axis = 0; axis < ld600x::AREA_AXIS_COUNT; axis++)
    EXPECT_FLOAT_EQ(dwell[axis].state, bounds[axis]) << "axis " << int(axis);
  EXPECT_FALSE(interference_x_min.has_state());
  EXPECT_FALSE(detection_x_min.has_state());
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
