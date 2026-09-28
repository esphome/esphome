#include "wifi_signal_sensor.h"
#ifdef USE_WIFI
#include "esphome/core/log.h"

namespace esphome::wifi_signal {

static const char *const TAG = "wifi_signal.sensor";

void WiFiSignalSensor::dump_config() { LOG_SENSOR("", "WiFi Signal", this); }

}  // namespace esphome::wifi_signal
#endif
