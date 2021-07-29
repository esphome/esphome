#include "wifi_info_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_info {

static const char *const TAG = "wifi_info";

void IPAddressWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WifiInfo IPAddress", this); }
void ScanResultsWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WifiInfo Scan Results", this); }
void SSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WifiInfo SSID", this); }
void BSSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WifiInfo BSSID", this); }
void MacAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "WifiInfo Mac Address", this); }

}  // namespace wifi_info
}  // namespace esphome
