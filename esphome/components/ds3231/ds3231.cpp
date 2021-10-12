#include "ds3231.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS1307.pdf

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231";

// DS3231 Register Addresses
static const uint8_t DS3231_REGISTER_ADDRESS_RTC = 0x00;
static const uint8_t DS3231_REGISTER_ADDRESS_ALARM = 0x07;
static const uint8_t DS3231_REGISTER_ADDRESS_CONTROL = 0x0E;
static const uint8_t DS3231_REGISTER_ADDRESS_STATUS = 0x0F;

// Alarm Type Bit Masks
static const uint8_t DS3231_MASK_ALARM_TYPE_M1 = 0x01;
static const uint8_t DS3231_MASK_ALARM_TYPE_M2 = 0x02;
static const uint8_t DS3231_MASK_ALARM_TYPE_M3 = 0x04;
static const uint8_t DS3231_MASK_ALARM_TYPE_M4 = 0x08;
static const uint8_t DS3231_MASK_ALARM_TYPE_DAY_MODE = 0x10;
static const uint8_t DS3231_MASK_ALARM_TYPE_INTERRUPT_ENABLE = 0x40;

void DS3231Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3231...");
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
  if(!this->read_alarm_()) {
    this->mark_failed();
  }
  if(!this->read_control_()) {
    this->mark_failed();
  }

  if(!this->read_status_()) {
    this->mark_failed();
  }
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS3231 failed!");
  }
}

void DS3231Component::set_alarm_1(DS3231Alarm1Type alarm_type, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day) {
  ds3231_.alrm.reg.a1_second = second % 10;
  ds3231_.alrm.reg.a1_second_10 = second / 10;
  ds3231_.alrm.reg.a1_m1 = alarm_type & DS3231_MASK_ALARM_TYPE_M1;
  ds3231_.alrm.reg.a1_minute = minute % 10;
  ds3231_.alrm.reg.a1_minute_10 = minute / 10;
  ds3231_.alrm.reg.a1_m2 = alarm_type & DS3231_MASK_ALARM_TYPE_M2;
  ds3231_.alrm.reg.a1_hour = hour % 10;
  ds3231_.alrm.reg.a1_hour_10 = hour / 10;
  ds3231_.alrm.reg.a1_m3 = alarm_type & DS3231_MASK_ALARM_TYPE_M3;
  ds3231_.alrm.reg.a1_day = day % 10;
  ds3231_.alrm.reg.a1_day_10 = day / 10;
  ds3231_.alrm.reg.a1_day_mode = alarm_type & DS3231_MASK_ALARM_TYPE_DAY_MODE;
  ds3231_.alrm.reg.a1_m4 = alarm_type & DS3231_MASK_ALARM_TYPE_M4;
  this->write_alarm_();
  if (ds3231_.ctrl.reg.alrm_1_int != bool(alarm_type & DS3231_MASK_ALARM_TYPE_INTERRUPT_ENABLE)) {
    ds3231_.ctrl.reg.alrm_1_int = bool(alarm_type & DS3231_MASK_ALARM_TYPE_INTERRUPT_ENABLE);
    this->write_control_();
  }
}

void DS3231Component::reset_alarm_1() {
  read_status_();
  if (ds3231_.stat.reg.alrm_1_act) {
    ds3231_.stat.reg.alrm_1_act = false;
    write_status_();
  }
}

void DS3231Component::set_alarm_2(DS3231Alarm2Type alarm_type, uint8_t minute, uint8_t hour, uint8_t day) {
  ds3231_.alrm.reg.a2_minute = minute % 10;
  ds3231_.alrm.reg.a2_minute_10 = minute / 10;
  ds3231_.alrm.reg.a2_m2 = alarm_type & DS3231_MASK_ALARM_TYPE_M2;
  ds3231_.alrm.reg.a2_hour = hour % 10;
  ds3231_.alrm.reg.a2_hour_10 = hour / 10;
  ds3231_.alrm.reg.a2_m3 = alarm_type & DS3231_MASK_ALARM_TYPE_M3;
  ds3231_.alrm.reg.a2_day = day % 10;
  ds3231_.alrm.reg.a2_day_10 = day / 10;
  ds3231_.alrm.reg.a2_day_mode = alarm_type & DS3231_MASK_ALARM_TYPE_DAY_MODE;
  ds3231_.alrm.reg.a2_m4 = alarm_type & DS3231_MASK_ALARM_TYPE_M4;
  this->write_alarm_();
  if (ds3231_.ctrl.reg.alrm_2_int != bool(alarm_type & DS3231_MASK_ALARM_TYPE_INTERRUPT_ENABLE)) {
    ds3231_.ctrl.reg.alrm_2_int = bool(alarm_type & DS3231_MASK_ALARM_TYPE_INTERRUPT_ENABLE);
    this->write_control_();
  }
}

void DS3231Component::reset_alarm_2() {
  read_status_();
  if (ds3231_.stat.reg.alrm_2_act) {
    ds3231_.stat.reg.alrm_2_act = false;
    write_status_();
  }
}

void DS3231Component::set_square_wave_mode(DS3231SquareWaveMode mode) {
  if (mode == DS3231SquareWaveMode::Interupt && !ds3231_.ctrl.reg.int_ctrl) {
    ds3231_.ctrl.reg.int_ctrl = true;
    this->write_control_();
  }
  else if (mode == DS3231SquareWaveMode::SquareWave && ds3231_.ctrl.reg.int_ctrl) {
    ds3231_.ctrl.reg.int_ctrl = false;
    this->write_control_();
  }
}

void DS3231Component::set_square_wave_frequency(DS3231SquareWaveFrequency frequency) {
  if (ds3231_.ctrl.reg.rs != frequency) {
    ds3231_.ctrl.reg.rs = frequency;
    this->write_control_();
  }
}

bool DS3231Component::read_rtc_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           ds3231_.rtc.reg.hour_10, ds3231_.rtc.reg.hour,
           ds3231_.rtc.reg.minute_10, ds3231_.rtc.reg.minute,
           ds3231_.rtc.reg.second_10, ds3231_.rtc.reg.second,
           ds3231_.rtc.reg.year_10, ds3231_.rtc.reg.year,
           ds3231_.rtc.reg.month_10, ds3231_.rtc.reg.month,
           ds3231_.rtc.reg.day_10, ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::write_rtc_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           ds3231_.rtc.reg.hour_10, ds3231_.rtc.reg.hour,
           ds3231_.rtc.reg.minute_10, ds3231_.rtc.reg.minute,
           ds3231_.rtc.reg.second_10, ds3231_.rtc.reg.second,
           ds3231_.rtc.reg.year_10, ds3231_.rtc.reg.year,
           ds3231_.rtc.reg.month_10, ds3231_.rtc.reg.month,
           ds3231_.rtc.reg.day_10, ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::read_alarm_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_ALARM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a1_hour_10, ds3231_.alrm.reg.a1_hour,
           ds3231_.alrm.reg.a1_minute_10, ds3231_.alrm.reg.a1_minute,
           ds3231_.alrm.reg.a1_second_10, ds3231_.alrm.reg.a1_second,
           ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a1_day_10, ds3231_.alrm.reg.a1_day,
           ds3231_.alrm.reg.a1_m1, ds3231_.alrm.reg.a1_m2, ds3231_.alrm.reg.a1_m3, ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Read  Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a2_hour_10, ds3231_.alrm.reg.a2_hour,
           ds3231_.alrm.reg.a2_minute_10, ds3231_.alrm.reg.a2_minute,
           ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a2_day_10, ds3231_.alrm.reg.a2_day,
           ds3231_.alrm.reg.a2_m2, ds3231_.alrm.reg.a2_m3, ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::write_alarm_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_ALARM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a1_hour_10, ds3231_.alrm.reg.a1_hour,
           ds3231_.alrm.reg.a1_minute_10, ds3231_.alrm.reg.a1_minute,
           ds3231_.alrm.reg.a1_second_10, ds3231_.alrm.reg.a1_second,
           ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a1_day_10, ds3231_.alrm.reg.a1_day,
           ds3231_.alrm.reg.a1_m1, ds3231_.alrm.reg.a1_m2, ds3231_.alrm.reg.a1_m3, ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Write Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u",
           ds3231_.alrm.reg.a2_hour_10, ds3231_.alrm.reg.a2_hour,
           ds3231_.alrm.reg.a2_minute_10, ds3231_.alrm.reg.a2_minute,
           ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW",
           ds3231_.alrm.reg.a2_day_10, ds3231_.alrm.reg.a2_day,
           ds3231_.alrm.reg.a2_m2, ds3231_.alrm.reg.a2_m3, ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::read_control_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_CONTROL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(ds3231_.ctrl.reg.alrm_1_int),
           ONOFF(ds3231_.ctrl.reg.alrm_2_int),
           ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW",
           ds3231_.ctrl.reg.rs,
           ONOFF(ds3231_.ctrl.reg.conv_tmp),
           ONOFF(ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::write_control_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_CONTROL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(ds3231_.ctrl.reg.alrm_1_int),
           ONOFF(ds3231_.ctrl.reg.alrm_2_int),
           ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW",
           ds3231_.ctrl.reg.rs,
           ONOFF(ds3231_.ctrl.reg.conv_tmp),
           ONOFF(ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::read_status_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_STATUS, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  A1:%s A2:%s BSY:%s 32K:%s OSC:%s",
           ONOFF(ds3231_.stat.reg.alrm_1_act),
           ONOFF(ds3231_.stat.reg.alrm_2_act),
           YESNO(ds3231_.stat.reg.busy),
           ONOFF(ds3231_.stat.reg.en32khz),
           ONOFF(!ds3231_.stat.reg.osc_stop));
  return true;
}

bool DS3231Component::write_status_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_STATUS, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write A1:%s A2:%s BSY:%s 32K:%s OSC:%s",
           ONOFF(ds3231_.stat.reg.alrm_1_act),
           ONOFF(ds3231_.stat.reg.alrm_2_act),
           YESNO(ds3231_.stat.reg.busy),
           ONOFF(ds3231_.stat.reg.en32khz),
           ONOFF(!ds3231_.stat.reg.osc_stop));
  return true;
}

}  // namespace ds3231
}  // namespace esphome
