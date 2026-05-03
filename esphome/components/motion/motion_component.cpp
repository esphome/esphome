#include "motion_component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace motion {

static const char *const TAG = "motion";

void MotionComponent::update() {
  if (this->is_failed())
    return;
  MotionData motion_data;
  {};
  MotionData raw_data{};
  if (!this->update_data_(raw_data))
    return;
  this->map_axes_(motion_data.acceleration, raw_data.acceleration);
  this->map_axes_(motion_data.angular_rate, raw_data.angular_rate);
  this->motion_data_callback_.call(motion_data);

  ESP_LOGV(TAG, "Accel: [%.3f, %.3f, %.3f] g; Gyro: [%.3f, %.3f, %.3f] °/s", motion_data.acceleration[X_AXIS],
           motion_data.acceleration[Y_AXIS], motion_data.acceleration[Z_AXIS], motion_data.angular_rate[X_AXIS],
           motion_data.angular_rate[Y_AXIS], motion_data.angular_rate[Z_AXIS]);
}

}  // namespace motion
}  // namespace esphome
