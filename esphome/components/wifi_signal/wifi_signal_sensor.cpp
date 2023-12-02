#include "wifi_signal_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_signal {

static const char *const TAG = "wifi_signal.sensor";

void WiFiSignalSensor::dump_config() { LOG_SENSOR("", "WiFi Signal", this); }

}  // namespace wifi_signal
}  // namespace esphome
