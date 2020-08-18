#include "epsolar.h"
#include "esphome/core/log.h"
#include <time.h>

namespace esphome {
namespace epsolar {

static const char *TAG = "epsolar";

void EPSOLAR::update() {
  if (this->sync_rtc_) {
    set_realtime_clock_to_now();
    this->sync_rtc_ =
        false;  // no need to sync the clock with every update. Todo :  does it make sense to próvide an action command?
  } else {
    ESP_LOGV(TAG, "====== RTC not changed ======");
  }
  ModbusComponent::update();
  ESP_LOGD(TAG, "EPSOLAR update complete");
}

void EPSOLAR::setup() {
  // Make sure base handler is called first
  ModbusComponent::setup();

  // Modify the returned data for the Working time length  1, 2 and  "Length of night register" where hour is pocked in
  // the uppper 8 bits and mins in the lower part if unit of measurement is 'h' return as hours else in minutes
  for (uint16_t adr : {0x903E, 0x903F, 0x9065}) {
    auto *item = find_by_register_address(adr);
    if (item != nullptr) {
      if (item->bitmask != 0xFFFF) {  // If the user specificed a non default bitmask leave it alone
        break;
      }
      // These registers are never binarysensors but just in case of a misconfig doublecheck
      if (SensorValueType::BIT == item->sensor_value_type) {
        return;
      }
      std::string unit = static_cast<FloatSensorItem *>(item)->sensor_->get_unit_of_measurement();
      // default to minutes
      float factor = 1.0;
      // return hours
      if (unit == "h") {
        factor = 60.0;
      }
      // Now replace the default handler for this sensor item to unpack the bits and convert to hours or minutes
      item->transform_expression = [factor](int64_t v) {
        int hours = (v & 0xFF00) >> 8;
        int mins = hours * 60 + (v & 0xFF);
        return mins / factor;
      };
    }
  }
}

void EPSOLAR::dump_config() {
  ModbusComponent::dump_config();
  ESP_LOGCONFIG(TAG, "sync RTC %s", this->sync_rtc_ ? "on" : "off");
}

void EPSOLAR::set_realtime_clock_to_now() {

  time_t now = ::time(nullptr);
  struct tm *time_info = ::localtime(&now);
  int seconds = time_info->tm_sec;
  int minutes = time_info->tm_min;
  int hour = time_info->tm_hour;
  int day = time_info->tm_mday;
  int month = time_info->tm_mon + 1;
  int year = time_info->tm_year % 100;
  // has to be static as well because we access the data out of scope

  std::vector<uint16_t> rtc_data = {uint16_t((minutes << 8) | seconds), uint16_t((day << 8) | hour),
                                    uint16_t((year << 8) | month)};
  ModbusCommandItem set_rtc_command = ModbusCommandItem::create_write_multiple_command(this, 0x9013, 3, rtc_data);
#if SIMULATE_ERROR
  set_rtc_command.register_address = 0;
  set_rtc_command.function_code = ModbusFunctionCode::kReadCoils;
  set_rtc_command.register_count = 200;
  set_rtc_command.expected_response_size = 1;
  set_rtc_command.payload.clear();
#endif
  this->queue_command_(set_rtc_command);
  ESP_LOGD(TAG, "EPSOLAR RTC set to %02d:%02d:%02d %02d.%02d.%04d", hour, minutes, seconds, day, month, year + 2000);
}

}  // namespace epsolar
}  // namespace esphome
