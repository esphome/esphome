#include "opentherm42_sensor_feed_number.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.number";

void OpenTherm42SensorFeedNumber::dump_config() { LOG_NUMBER("", "OpenTherm 4.2 Number", this); }

}  // namespace esphome::opentherm42
