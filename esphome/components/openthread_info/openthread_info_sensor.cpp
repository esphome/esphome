#include "openthread_info_sensor.h"
#ifdef USE_OPENTHREAD
#include "esphome/core/log.h"

namespace esphome::openthread_info {

static const char *const TAG = "openthread_info";

void ParentAverageRssiOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Average RSSI", this); }
void ParentLastRssiOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Last RSSI", this); }
void ParentLinkQualityInOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Link Quality In", this); }
void ParentLinkQualityOutOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Link Quality Out", this); }

}  // namespace esphome::openthread_info
#endif
