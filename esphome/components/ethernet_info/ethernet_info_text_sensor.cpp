#include "ethernet_info_text_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace ethernet_info {

static const char *const TAG = "ethernet_info";

void IPAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo IPAddress", this); }

}  // namespace ethernet_info
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
