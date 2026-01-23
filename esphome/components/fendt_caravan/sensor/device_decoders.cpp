#include "device_decoders.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
static const char *const TAG = "FC.COD";

time_t DeviceDecoders::decode_date(const std::string &data) {
  std::istringstream date(data);
  tm tm = {};
  date >> get_time(&tm, "%d.%m.%y");
  if (date.fail()) {
    ESP_LOGE(TAG, "Date Parsing failed");
    return 0;
  }
  time_t ret = mktime(&tm);
  return ret;
}
time_t DeviceDecoders::decode_time(const std::string &data) {
  std::istringstream date(data);
  tm tm = {};
  date >> get_time(&tm, "%H:%M:%S");
  if (date.fail()) {
    ESP_LOGE(TAG, "Date Parsing failed");
    return 0;
  }
  time_t ret = mktime(&tm);
  return ret;
}

}  // namespace esphome::fendt_caravan
#endif
