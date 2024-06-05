#include "ethernet_info_text_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ethernet_info {

static const char *const TAG = "ethernet_info";

void IPAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo IPAddress", this); }
void DNSAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo DNS Address", this); }
void MACAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo MAC Address", this); }

}  // namespace ethernet_info
}  // namespace esphome

#endif  // USE_ESP32
