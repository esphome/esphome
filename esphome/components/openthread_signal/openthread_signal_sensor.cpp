#include "openthread_signal_sensor.h"
#ifdef USE_OPENTHREAD
#include "esphome/core/log.h"

namespace esphome::openthread_signal {

static const char *const TAG = "openthread_signal.sensor";

void OpenThreadParentLqiInSensor::dump_config() { LOG_SENSOR("", "OpenThread Parent Link Quality In", this); }
void OpenThreadParentLqiOutSensor::dump_config() { LOG_SENSOR("", "OpenThread Parent Link Quality Out", this); }
void OpenThreadParentPathCostSensor::dump_config() { LOG_SENSOR("", "OpenThread Parent Path Cost", this); }
void OpenThreadParentRssiSensor::dump_config() { LOG_SENSOR("", "OpenThread Parent RSSI Last", this); }
void OpenThreadParentRssiAvgSensor::dump_config() { LOG_SENSOR("", "OpenThread Parent RSSI Average", this); }
void OpenThreadRssiSensor::dump_config() { LOG_SENSOR("", "OpenThread RSSI", this); }

}  // namespace esphome::openthread_signal
#endif
