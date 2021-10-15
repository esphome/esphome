#include "ds3231.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS3231.pdf

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231";

// DS3231 Register Addresses
static const uint8_t DS3231_REGISTER_ADDRESS_RTC = 0x00;
static const uint8_t DS3231_REGISTER_ADDRESS_ALARM = 0x07;
static const uint8_t DS3231_REGISTER_ADDRESS_CONTROL = 0x0E;
static const uint8_t DS3231_REGISTER_ADDRESS_STATUS = 0x0F;
static const uint8_t DS3231_REGISTER_ADDRESS_TEMP = 0x11;

// Alarm Type Bit Masks
static const uint8_t DS3231_MASK_ALARM_TYPE_M1 = 0x01;
static const uint8_t DS3231_MASK_ALARM_TYPE_M2 = 0x02;
static const uint8_t DS3231_MASK_ALARM_TYPE_M3 = 0x04;
static const uint8_t DS3231_MASK_ALARM_TYPE_M4 = 0x08;
static const uint8_t DS3231_MASK_ALARM_TYPE_DAY_MODE = 0x10;

void DS3231Component::set_default_alarm_1(DS3231Alarm1Mode mode, bool int_enabled, uint8_t second, uint8_t minute,
                                          uint8_t hour, uint8_t day) {
  this->alarm_1_mode_ = mode;
  this->alarm_1_interrupt_anabled_ = int_enabled;
  this->alarm_1_second_ = second;
  this->alarm_1_minute_ = minute;
  this->alarm_1_hour_ = hour;
  this->alarm_1_day_ = day;
}

void DS3231Component::set_default_alarm_2(DS3231Alarm2Mode mode, bool int_enabled, uint8_t minute, uint8_t hour,
                                          uint8_t day) {
  this->alarm_2_mode_ = mode;
  this->alarm_2_interrupt_anabled_ = int_enabled;
  this->alarm_2_minute_ = minute;
  this->alarm_2_hour_ = hour;
  this->alarm_2_day_ = day;
}

void DS3231Component::add_on_alarm_callback(std::function<void(uint8_t)> &&callback) {
  this->alarm_callback_.add(std::move(callback));
}

void DS3231Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3231...");
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
  if (!this->read_alarm_()) {
    this->mark_failed();
  }
  if (!this->read_control_()) {
    this->mark_failed();
  }
  if (!this->read_status_()) {
    this->mark_failed();
  }

  if (this->alarm_1_mode_.has_value()) {
    this->set_alarm_1(this->alarm_1_mode_.value(), this->alarm_1_interrupt_anabled_, this->alarm_1_second_,
                      this->alarm_1_minute_, this->alarm_1_hour_, this->alarm_1_day_);
  }
  if (this->alarm_2_mode_.has_value()) {
    this->set_alarm_2(this->alarm_2_mode_.value(), this->alarm_2_interrupt_anabled_, this->alarm_2_minute_,
                      this->alarm_2_hour_, this->alarm_2_day_);
  }
  if (this->square_wave_frequency_.has_value()) {
    this->set_square_wave_frequency(this->square_wave_frequency_.value());
  }
  if (this->square_wave_mode_.has_value()) {
    this->set_square_wave_mode(this->square_wave_mode_.value());
  }
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS3231 failed!");
  }
}

void DS3231Component::set_alarm_1(DS3231Alarm1Mode mode, bool int_enabled, uint8_t second, uint8_t minute, uint8_t hour,
                                  uint8_t day) {
  this->ds3231_.alrm.reg.a1_second = second % 10;
  this->ds3231_.alrm.reg.a1_second_10 = second / 10;
  this->ds3231_.alrm.reg.a1_m1 = mode & DS3231_MASK_ALARM_TYPE_M1;
  this->ds3231_.alrm.reg.a1_minute = minute % 10;
  this->ds3231_.alrm.reg.a1_minute_10 = minute / 10;
  this->ds3231_.alrm.reg.a1_m2 = mode & DS3231_MASK_ALARM_TYPE_M2;
  this->ds3231_.alrm.reg.a1_hour = hour % 10;
  this->ds3231_.alrm.reg.a1_hour_10 = hour / 10;
  this->ds3231_.alrm.reg.a1_m3 = mode & DS3231_MASK_ALARM_TYPE_M3;
  this->ds3231_.alrm.reg.a1_day = day % 10;
  this->ds3231_.alrm.reg.a1_day_10 = day / 10;
  this->ds3231_.alrm.reg.a1_day_mode = mode & DS3231_MASK_ALARM_TYPE_DAY_MODE;
  this->ds3231_.alrm.reg.a1_m4 = mode & DS3231_MASK_ALARM_TYPE_M4;
  this->write_alarm_();
  if (this->ds3231_.ctrl.reg.alrm_1_int != int_enabled) {
    this->ds3231_.ctrl.reg.alrm_1_int = int_enabled;
    this->write_control_();
  }
}

void DS3231Component::reset_alarm_1() {
  this->ds3231_.stat.reg.alrm_1_act = false;
  this->write_status_();
}

void DS3231Component::set_alarm_2(DS3231Alarm2Mode mode, bool int_enabled, uint8_t minute, uint8_t hour, uint8_t day) {
  this->ds3231_.alrm.reg.a2_minute = minute % 10;
  this->ds3231_.alrm.reg.a2_minute_10 = minute / 10;
  this->ds3231_.alrm.reg.a2_m2 = mode & DS3231_MASK_ALARM_TYPE_M2;
  this->ds3231_.alrm.reg.a2_hour = hour % 10;
  this->ds3231_.alrm.reg.a2_hour_10 = hour / 10;
  this->ds3231_.alrm.reg.a2_m3 = mode & DS3231_MASK_ALARM_TYPE_M3;
  this->ds3231_.alrm.reg.a2_day = day % 10;
  this->ds3231_.alrm.reg.a2_day_10 = day / 10;
  this->ds3231_.alrm.reg.a2_day_mode = mode & DS3231_MASK_ALARM_TYPE_DAY_MODE;
  this->ds3231_.alrm.reg.a2_m4 = mode & DS3231_MASK_ALARM_TYPE_M4;
  this->write_alarm_();
  if (this->ds3231_.ctrl.reg.alrm_2_int != int_enabled) {
    this->ds3231_.ctrl.reg.alrm_2_int = int_enabled;
    this->write_control_();
  }
}

void DS3231Component::reset_alarm_2() {
  this->ds3231_.stat.reg.alrm_2_act = false;
  this->write_status_();
}

void DS3231Component::set_square_wave_mode(DS3231SquareWaveMode mode) {
  if (mode == DS3231SquareWaveMode::MODE_ALARM_INTERRUPT && !this->ds3231_.ctrl.reg.int_ctrl) {
    this->ds3231_.ctrl.reg.int_ctrl = true;
    this->write_control_();
  } else if (mode == DS3231SquareWaveMode::MODE_SQUARE_WAVE && this->ds3231_.ctrl.reg.int_ctrl) {
    this->ds3231_.ctrl.reg.int_ctrl = false;
    this->write_control_();
  }
}

void DS3231Component::set_square_wave_frequency(DS3231SquareWaveFrequency frequency) {
  if (this->ds3231_.ctrl.reg.rs != frequency) {
    this->ds3231_.ctrl.reg.rs = frequency;
    this->write_control_();
  }
}

bool DS3231Component::read_rtc_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read Time - %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u", this->ds3231_.rtc.reg.hour_10,
           this->ds3231_.rtc.reg.hour, this->ds3231_.rtc.reg.minute_10, this->ds3231_.rtc.reg.minute,
           this->ds3231_.rtc.reg.second_10, this->ds3231_.rtc.reg.second, this->ds3231_.rtc.reg.year_10,
           this->ds3231_.rtc.reg.year, this->ds3231_.rtc.reg.month_10, this->ds3231_.rtc.reg.month,
           this->ds3231_.rtc.reg.day_10, this->ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::write_rtc_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_RTC, this->ds3231_.rtc.raw, sizeof(this->ds3231_.rtc.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Time - %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u", this->ds3231_.rtc.reg.hour_10,
           this->ds3231_.rtc.reg.hour, this->ds3231_.rtc.reg.minute_10, this->ds3231_.rtc.reg.minute,
           this->ds3231_.rtc.reg.second_10, this->ds3231_.rtc.reg.second, this->ds3231_.rtc.reg.year_10,
           this->ds3231_.rtc.reg.year, this->ds3231_.rtc.reg.month_10, this->ds3231_.rtc.reg.month,
           this->ds3231_.rtc.reg.day_10, this->ds3231_.rtc.reg.day);
  return true;
}

bool DS3231Component::read_alarm_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_ALARM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           this->ds3231_.alrm.reg.a1_hour_10, this->ds3231_.alrm.reg.a1_hour, this->ds3231_.alrm.reg.a1_minute_10,
           this->ds3231_.alrm.reg.a1_minute, this->ds3231_.alrm.reg.a1_second_10, this->ds3231_.alrm.reg.a1_second,
           this->ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW", this->ds3231_.alrm.reg.a1_day_10,
           this->ds3231_.alrm.reg.a1_day, this->ds3231_.alrm.reg.a1_m1, this->ds3231_.alrm.reg.a1_m2,
           this->ds3231_.alrm.reg.a1_m3, this->ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Read Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u", this->ds3231_.alrm.reg.a2_hour_10,
           this->ds3231_.alrm.reg.a2_hour, this->ds3231_.alrm.reg.a2_minute_10, this->ds3231_.alrm.reg.a2_minute,
           this->ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW", this->ds3231_.alrm.reg.a2_day_10,
           this->ds3231_.alrm.reg.a2_day, this->ds3231_.alrm.reg.a2_m2, this->ds3231_.alrm.reg.a2_m3,
           this->ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::write_alarm_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_ALARM, this->ds3231_.alrm.raw, sizeof(this->ds3231_.alrm.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Alarm1 - %0u%0u:%0u%0u:%0u%0u %s:%0u%0u M1:%0u M2:%0u M3:%0u M4:%0u",
           this->ds3231_.alrm.reg.a1_hour_10, this->ds3231_.alrm.reg.a1_hour, this->ds3231_.alrm.reg.a1_minute_10,
           this->ds3231_.alrm.reg.a1_minute, this->ds3231_.alrm.reg.a1_second_10, this->ds3231_.alrm.reg.a1_second,
           this->ds3231_.alrm.reg.a1_day_mode == 0 ? "DoM" : "DoW", this->ds3231_.alrm.reg.a1_day_10,
           this->ds3231_.alrm.reg.a1_day, this->ds3231_.alrm.reg.a1_m1, this->ds3231_.alrm.reg.a1_m2,
           this->ds3231_.alrm.reg.a1_m3, this->ds3231_.alrm.reg.a1_m4);
  ESP_LOGD(TAG, "Write Alarm2 - %0u%0u:%0u%0u %s:%0u%0u M2:%0u M3:%0u M4:%0u", this->ds3231_.alrm.reg.a2_hour_10,
           this->ds3231_.alrm.reg.a2_hour, this->ds3231_.alrm.reg.a2_minute_10, this->ds3231_.alrm.reg.a2_minute,
           this->ds3231_.alrm.reg.a2_day_mode == 0 ? "DoM" : "DoW", this->ds3231_.alrm.reg.a2_day_10,
           this->ds3231_.alrm.reg.a2_day, this->ds3231_.alrm.reg.a2_m2, this->ds3231_.alrm.reg.a2_m3,
           this->ds3231_.alrm.reg.a2_m4);
  return true;
}

bool DS3231Component::read_control_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_CONTROL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read Control - A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(this->ds3231_.ctrl.reg.alrm_1_int), ONOFF(this->ds3231_.ctrl.reg.alrm_2_int),
           this->ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW", this->ds3231_.ctrl.reg.rs,
           ONOFF(this->ds3231_.ctrl.reg.conv_tmp), ONOFF(this->ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!this->ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::write_control_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_CONTROL, this->ds3231_.ctrl.raw, sizeof(this->ds3231_.ctrl.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Control -  A1I:%s A2I:%s INT_SQW:%s RS:%0u CT:%s BSQW:%s OSC:%s",
           ONOFF(this->ds3231_.ctrl.reg.alrm_1_int), ONOFF(this->ds3231_.ctrl.reg.alrm_2_int),
           this->ds3231_.ctrl.reg.int_ctrl ? "INT" : "SQW", this->ds3231_.ctrl.reg.rs,
           ONOFF(this->ds3231_.ctrl.reg.conv_tmp), ONOFF(this->ds3231_.ctrl.reg.bat_sqw),
           ONOFF(!this->ds3231_.ctrl.reg.osc_dis));
  return true;
}

bool DS3231Component::read_status_(bool initial_read) {
  bool alarm_1_act = this->ds3231_.stat.reg.alrm_1_act;
  bool alarm_2_act = this->ds3231_.stat.reg.alrm_2_act;

  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_STATUS, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read Status - A1:%s A2:%s BSY:%s 32K:%s OSC:%s", ONOFF(this->ds3231_.stat.reg.alrm_1_act),
           ONOFF(this->ds3231_.stat.reg.alrm_2_act), YESNO(this->ds3231_.stat.reg.busy),
           ONOFF(this->ds3231_.stat.reg.en32khz), ONOFF(!this->ds3231_.stat.reg.osc_stop));

  if (!initial_read && alarm_1_act != this->ds3231_.stat.reg.alrm_1_act) {
    this->alarm_callback_.call(1);
  }
  if (!initial_read && alarm_2_act != this->ds3231_.stat.reg.alrm_2_act) {
    this->alarm_callback_.call(2);
  }

  return true;
}

bool DS3231Component::write_status_() {
  if (!this->write_bytes(DS3231_REGISTER_ADDRESS_STATUS, this->ds3231_.stat.raw, sizeof(this->ds3231_.stat.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write Status - A1:%s A2:%s BSY:%s 32K:%s OSC:%s", ONOFF(this->ds3231_.stat.reg.alrm_1_act),
           ONOFF(this->ds3231_.stat.reg.alrm_2_act), YESNO(this->ds3231_.stat.reg.busy),
           ONOFF(this->ds3231_.stat.reg.en32khz), ONOFF(!this->ds3231_.stat.reg.osc_stop));
  return true;
}

bool DS3231Component::read_temperature_() {
  if (!this->read_bytes(DS3231_REGISTER_ADDRESS_TEMP, this->ds3231_.temp.raw, sizeof(this->ds3231_.temp.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read Temperature - INT:%d FRAC:%d", this->ds3231_.temp.reg.upper, this->ds3231_.temp.reg.lower);
  return true;
}

}  // namespace ds3231
}  // namespace esphome
