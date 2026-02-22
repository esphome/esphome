
#include "openthread_signal_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_OPENTHREAD

namespace esphome::openthread_signal {

static const char *const TAG = "openthread_signal";

void ParentAverageRssiOpenThreadSignal::dump_config() { LOG_SENSOR("", "Parent average RSSI", this); }
void ParentLastRssiOpenThreadSignal::dump_config() { LOG_SENSOR("", "Parent last RSSI", this); }
void BaseLinkCounterOpenThreadSignal::dump_config() {
  LOG_SENSOR("", this->info_name_ == nullptr ? "" : this->info_name_, this);
}

}  // namespace esphome::openthread_signal
#endif
