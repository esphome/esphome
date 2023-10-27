#include "esphome/components/ld2450/target.h"

#include "esphome/components/ld2450/defines.h"

#include <cmath>  // for M_PI
#include <string>

namespace esphome {
namespace ld2450 {

void Target::update_from_buffer(const uint8_t *record) {
#ifdef USE_TARGET_COORDS
  x_ = convert_signed(record);
  y_ = convert_signed(record + 2);
#endif
#ifdef USE_TARGET_SPEED
  speed_ = convert_signed(record + 4);
#endif
#ifdef USE_TARGET_RESOLUTION
  resolution_ = convert_unsigned(record + 6);
#endif
  valid_ = (*reinterpret_cast<const uint64_t *>(record) != 0);
}

void Target::publish() const {
#ifdef USE_TARGET_COORDS
  PUBLISH_SENSOR(x, x_)
  PUBLISH_SENSOR(y, y_)
  PUBLISH_SENSOR(angle, get_angle())
#endif

#ifdef USE_TARGET_SPEED
  PUBLISH_SENSOR(speed, speed_)
  PUBLISH_TEXT_SENSOR(direction, get_direction())
#endif

#ifdef USE_TARGET_RESOLUTION
  PUBLISH_SENSOR(resolution, resolution_)
  PUBLISH_TEXT_SENSOR(direction, get_direction())
#endif
}

#ifdef USE_TARGET_COORDS
float Target::get_angle() const {
  static const float RADIAN_TO_DEGREES = 180. / M_PI;

  return valid_ ? (float) x_ / y_ * RADIAN_TO_DEGREES : 0.f;
}
#endif

#ifdef USE_TARGET_SPEED
const std::string &Target::get_position() const {
  static const std::string MOVING_AWAY = "Moving away";
  static const std::string APPROACHING = "Approaching";
  static const std::string STATIC = "Static";

  if (speed_ == 0)
    return STATIC;
  if (speed_ > 0)
    return MOVING_AWAY;
  return APPROACHING;
}
#endif

#ifdef USE_TARGET_RESOLUTION
const std::string &Target::get_direction() const {
  static const std::string NONE = "None";
  static const std::string LEFT = "Left";
  static const std::string RIGHT = "Right";
  static const std::string MIDDLE = "Middle";

  if (x_ > 0)
    return RIGHT;
  if (x_ < 0)
    return LEFT;
  if (y_ != 0)
    return MIDDLE;

  return NONE;
}
#endif

}  // namespace ld2450
}  // namespace esphome
