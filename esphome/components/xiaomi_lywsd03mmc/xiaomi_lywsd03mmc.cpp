#include "xiaomi_lywsd03mmc.h"
//#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_lywsd03mmc {

//static const char *TAG = "xiaomi_lywsd03mmc";
std::string hexencode_string(const std::string &raw_data)
{
  return hexencode(reinterpret_cast<const uint8_t *>(raw_data.c_str()), raw_data.size());
}
void XiaomiLYWSD03MMC::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSD03MMC");
  ESP_LOGCONFIG(TAG, "BindKey");
  ESP_LOGCONFIG(TAG,hexencode_string(std::string(reinterpret_cast<const char *>(bindkey_), 16)).c_str() );
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_lywsd03mmc
}  // namespace esphome

#endif
