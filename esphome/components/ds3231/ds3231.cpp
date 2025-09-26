#include "ds3231.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/application.h"
#include <cstring>

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231";

// Register addresses
static const uint8_t DS3231_REG_TIME = 0x00;
static const uint8_t DS3231_REG_CONTROL = 0x0E;
static const uint8_t DS3231_REG_STATUS = 0x0F;
static const uint8_t DS3231_REG_TEMP = 0x11;

// Helper functions for the class
namespace {
int clamp(int value, int min_val, int max_val) {
  return (value < min_val) ? min_val : (value > max_val) ? max_val : value;
}

int get_days_in_month(int month, int year) {
  static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12)
    return 31;

  int days = days_in_month[month - 1];
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    days = 29;
  }
  return days;
}

ESPTime create_valid_time(int second, int minute, int hour, int day_of_week, int day, int month, int year) {
  ESPTime time;
  time.second = clamp(second, 0, 59);
  time.minute = clamp(minute, 0, 59);
  time.hour = clamp(hour, 0, 23);
  time.day_of_week = clamp(day_of_week, 1, 7);
  time.day_of_month = clamp(day, 1, get_days_in_month(month, year));
  time.month = clamp(month, 1, 12);
  time.year = clamp(year, 2000, 2100);
  time.recalc_timestamp_utc(true);
  return time;
}
}  // namespace

void DS3231Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3231 at address 0x%02X", this->address_);

  // Check I2C communication
  if (!this->read_register(DS3231_REG_TIME)) {
    ESP_LOGE(TAG, "DS3231 not found at I2C address 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Clear oscillator stop flag and enable oscillator
  uint8_t status = this->read_register(DS3231_REG_STATUS);
  if (status & 0x80) {
    ESP_LOGW(TAG, "Oscillator was stopped, clearing status");
    this->write_register(DS3231_REG_STATUS, status & ~0x80);
  }

  // Enable oscillator (clear EOSC bit) and disable square wave output
  this->write_register(DS3231_REG_CONTROL, 0x00);

  // Debug info
  this->debug_registers();

  // Read initial time
  this->update_time_text_sensor();
}

void DS3231Component::update() {
  if (this->is_failed()) {
    // Attempt recovery if device was marked as failed
    this->attempt_recovery();
    return;
  }

  this->update_time_text_sensor();

  if (this->temperature_sensor_ != nullptr) {
    this->read_temperature();
  }
}

void DS3231Component::attempt_recovery() {
  static uint32_t last_recovery_attempt = 0;
  uint32_t now = millis();

  // Try to recover every 30 seconds
  if (now - last_recovery_attempt > 30000) {
    ESP_LOGW(TAG, "Attempting to recover DS3231 communication");
    last_recovery_attempt = now;

    // Check if device responds
    if (this->read_register(DS3231_REG_TIME)) {
      ESP_LOGI(TAG, "DS3231 recovered successfully");
      this->mark_failed();  // Clear failed state
      this->setup();        // Reinitialize
    } else {
      ESP_LOGE(TAG, "DS3231 recovery failed");
    }
  }
}

void DS3231Component::update_time_text_sensor() {
  if (this->time_text_sensor_ != nullptr) {
    this->time_text_sensor_->publish_state(this->get_rtc_time_str());
  }
}

std::string DS3231Component::get_rtc_time_str() {
  ESPTime time = this->get_rtc_time();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);
  return std::string(buffer);
}

ESPTime DS3231Component::get_rtc_time() {
  uint8_t data[7];

  if (!this->read_bytes(DS3231_REG_TIME, data, 7)) {
    ESP_LOGE(TAG, "Failed to read time from DS3231");
    return create_valid_time(0, 0, 12, 1, 1, 1, 2000);
  }

  // Parse time registers
  int second = this->bcd_to_dec(data[0] & 0x7F);
  int minute = this->bcd_to_dec(data[1] & 0x7F);
  int hour = this->bcd_to_dec(data[2] & 0x3F);

  // Handle 12-hour mode
  if (data[2] & 0x40) {
    hour = this->bcd_to_dec(data[2] & 0x1F);
    if (data[2] & 0x20) {  // PM
      hour = (hour % 12) + 12;
    } else {  // AM
      hour = hour % 12;
    }
  }

  int day_of_week = this->bcd_to_dec(data[3] & 0x07);
  int day = this->bcd_to_dec(data[4] & 0x3F);
  int month = this->bcd_to_dec(data[5] & 0x1F);
  int year = this->bcd_to_dec(data[6]) + 2000;

  // Handle century bit (corrected implementation)
  if (data[5] & 0x80) {
    year += 100;  // Add 100 years if century bit is set
  }

  return create_valid_time(second, minute, hour, day_of_week, day, month, year);
}

void DS3231Component::update_rtc_time() {
  if (this->time_id_ == nullptr) {
    ESP_LOGE(TAG, "No time source configured");
    return;
  }

  ESPTime sntp_time = this->time_id_->now();
  if (!sntp_time.is_valid()) {
    ESP_LOGE(TAG, "SNTP time is not valid");
    return;
  }

  ESP_LOGI(TAG, "Updating DS3231 from SNTP: %04d-%02d-%02d %02d:%02d:%02d", sntp_time.year, sntp_time.month,
           sntp_time.day_of_month, sntp_time.hour, sntp_time.minute, sntp_time.second);

  if (this->write_time()) {
    ESP_LOGI(TAG, "DS3231 updated successfully");
    this->update_time_text_sensor();
  } else {
    ESP_LOGE(TAG, "Failed to update DS3231");
  }
}

bool DS3231Component::update_rtc_manual_time(int year, int month, int day, int hour, int minute, int second) {
  // Validate input parameters
  if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) {
    ESP_LOGE(TAG, "Invalid time parameters");
    return false;
  }

  ESP_LOGI(TAG, "Setting manual time: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

  if (this->write_manual_time(year, month, day, hour, minute, second)) {
    ESP_LOGI(TAG, "Manual time set successfully");
    this->update_time_text_sensor();
    return true;
  }

  ESP_LOGE(TAG, "Failed to set manual time");
  return false;
}

bool DS3231Component::read_time() { return this->get_rtc_time().is_valid(); }

bool DS3231Component::write_time() {
  if (this->time_id_ == nullptr)
    return false;

  ESPTime now = this->time_id_->now();
  return now.is_valid()
             ? this->write_manual_time(now.year, now.month, now.day_of_month, now.hour, now.minute, now.second)
             : false;
}

bool DS3231Component::write_manual_time(int year, int month, int day, int hour, int minute, int second) {
  // Calculate day of week using Zeller's congruence
  int m = month;
  int y = year;
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  int century = y / 100;
  int year_of_century = y % 100;
  int day_of_week = (day + (13 * (m + 1)) / 5 + year_of_century + year_of_century / 4 + century / 4 + 5 * century) % 7;
  day_of_week = (day_of_week + 5) % 7 + 1;  // Convert to DS3231 format (1=Sun, 7=Sat)

  // Prepare month register with century bit
  uint8_t month_reg = this->dec_to_bcd(month);
  if (year >= 2100) {
    month_reg &= 0x7F;  // Clear century bit for 2100+
  } else {
    month_reg |= 0x80;  // Set century bit for 2000-2099
  }

  uint8_t data[7] = {
      this->dec_to_bcd(second),
      this->dec_to_bcd(minute),
      this->dec_to_bcd(hour),  // Always 24-hour mode
      this->dec_to_bcd(day_of_week),
      this->dec_to_bcd(day),
      month_reg,                    // Month with century bit
      this->dec_to_bcd(year % 100)  // Last two digits of year
  };

  bool success = this->write_bytes(DS3231_REG_TIME, data, 7);

  if (success) {
    ESP_LOGD(TAG, "Time written: %04d-%02d-%02d %02d:%02d:%02d (DOW: %d)", year, month, day, hour, minute, second,
             day_of_week);
  }

  return success;
}

bool DS3231Component::read_temperature() {
  uint8_t data[2];
  if (!this->read_bytes(DS3231_REG_TEMP, data, 2)) {
    return false;
  }

  int16_t temp_raw = (data[0] << 8) | data[1];
  float temperature = (temp_raw >> 6) * 0.25f;

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  return true;
}

uint8_t DS3231Component::read_register(uint8_t reg) {
  uint8_t data = 0;
  this->read_byte(reg, &data);
  return data;
}

void DS3231Component::write_register(uint8_t reg, uint8_t value) { this->write_byte(reg, value); }

void DS3231Component::debug_registers() {
  uint8_t data[19];
  if (this->read_bytes(0x00, data, 19)) {
    ESP_LOGI(TAG, "Register Dump:");
    ESP_LOGI(TAG, "Time:    %02X %02X %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3], data[4], data[5],
             data[6]);
    ESP_LOGI(TAG, "Control: %02X, Status: %02X", data[14], data[15]);
    ESP_LOGI(TAG, "Temp:    %02X %02X", data[17], data[18]);
  }
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }

  if (this->time_text_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Time sensor: configured");
  }

  if (this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature sensor: configured");
  }

  // Log update interval
  uint32_t interval = this->get_update_interval();
  if (interval == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(TAG, "  Update: disabled");
  } else {
    ESP_LOGCONFIG(TAG, "  Update: %.1f seconds", interval / 1000.0f);
  }
}

}  // namespace ds3231
}  // namespace esphome
