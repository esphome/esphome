#include "ds3231.h"
#include "esphome/core/log.h"

// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231";

void DS3231Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3231...");
  
  if (!this->read_rtc_()) {
    ESP_LOGE(TAG, "Communication with DS3231 failed!");
    this->mark_failed();
    return;
  }

  // Clear oscillator stop flag if set
  if (ds3231_reg_.status & 0x80) {
    ESP_LOGW(TAG, "Oscillator was stopped, clearing status flag");
    ds3231_reg_.status &= ~0x80;
    this->write_rtc_();
  }

  // Enable oscillator (set EOSC=0) and basic configuration
  ds3231_reg_.control = 0x00;  // Enable oscillator, square wave disabled

  this->write_rtc_();

  ESP_LOGD(TAG, "DS3231 initialized successfully");
}

void DS3231Component::update() { 
  this->read_time(); 
  
  // Read temperature if sensor is configured
  if (this->temperature_sensor_ != nullptr && this->read_rtc_()) {
    int8_t temp_whole = ds3231_reg_.temp_msb;
    uint8_t temp_frac = (ds3231_reg_.temp_lsb >> 6) * 25; // 0.25°C steps
    float temperature = temp_whole + (temp_frac / 100.0f);
    
    this->temperature_sensor_->publish_state(temperature);
    ESP_LOGD(TAG, "Temperature: %.2f°C", temperature);
  }
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication failed!");
    return;
  }
  
  if (this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature sensor: enabled");
  }
}

float DS3231Component::get_setup_priority() const { 
  return setup_priority::DATA; 
}

void DS3231Component::read_time() {
  if (!this->read_rtc_()) {
    return;
  }

  // Extract time components from BCD
  uint8_t seconds = this->bcd_to_bin_(ds3231_reg_.seconds & 0x7F);
  uint8_t minutes = this->bcd_to_bin_(ds3231_reg_.minutes & 0x7F);
  uint8_t hours = this->bcd_to_bin_(ds3231_reg_.hours & 0x3F);
  uint8_t day_of_month = this->bcd_to_bin_(ds3231_reg_.day & 0x3F);
  uint8_t month = this->bcd_to_bin_(ds3231_reg_.month & 0x1F);
  uint16_t year = this->bcd_to_bin_(ds3231_reg_.year) + 2000;

  // Handle 12-hour mode
  if (ds3231_reg_.hours & 0x40) {
    hours = this->bcd_to_bin_(ds3231_reg_.hours & 0x1F);
    if (ds3231_reg_.hours & 0x20) {
      hours = (hours % 12) + 12; // PM
    } else {
      hours = hours % 12; // AM
    }
  }

  // Apply century bit
  if (ds3231_reg_.month & 0x80) {
    year += 100;  // 2100-2199
  }

  ESPTime rtc_time{
      .second = seconds,
      .minute = minutes,
      .hour = hours,
      .day_of_week = this->bcd_to_bin_(ds3231_reg_.day_of_week & 0x07),
      .day_of_month = day_of_month,
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = month,
      .year = year,
      .is_dst = false,  // not used
      .timestamp = 0    // overwritten by recalc_timestamp_utc(false)
  };

  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time: %04d-%02d-%02d %02d:%02d:%02d", 
             rtc_time.year, rtc_time.month, rtc_time.day_of_month,
             rtc_time.hour, rtc_time.minute, rtc_time.second);
    return;
  }

  // Sync with system time
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);

  ESP_LOGD(TAG, "RTC time: %04d-%02d-%02d %02d:%02d:%02d (DOW: %d)", 
           rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second,
           rtc_time.day_of_week);
}

void DS3231Component::write_time() {
  auto now = this->now();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not writing to RTC");
    return;
  }

  // Calculate day of week (DS3231 uses 1-7 where 1=Sunday)
  int y = now.year;
  int m = now.month;
  int d = now.day_of_month;
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  int century = y / 100;
  int year_of_century = y % 100;
  int day_of_week = (d + (13 * (m + 1)) / 5 + year_of_century + year_of_century / 4 + century / 4 + 5 * century) % 7;
  day_of_week = (day_of_week + 5) % 7 + 1;  // Convert to 1=Sunday

  // Convert to BCD and set registers
  ds3231_reg_.seconds = this->bin_to_bcd_(now.second);
  ds3231_reg_.minutes = this->bin_to_bcd_(now.minute);
  ds3231_reg_.hours = this->bin_to_bcd_(now.hour);  // 24-hour mode
  ds3231_reg_.day_of_week = this->bin_to_bcd_(day_of_week);
  ds3231_reg_.day = this->bin_to_bcd_(now.day_of_month);
  
  // Set month with century bit
  ds3231_reg_.month = this->bin_to_bcd_(now.month);
  if (now.year >= 2100) {
    ds3231_reg_.month &= 0x7F;  // Clear century bit for 2100+
  } else {
    ds3231_reg_.month |= 0x80;  // Set century bit for 2000-2099
  }
  
  ds3231_reg_.year = this->bin_to_bcd_(now.year % 100);

  // Clear oscillator stop flag
  ds3231_reg_.status &= ~0x80;

  if (!this->write_rtc_()) {
    ESP_LOGE(TAG, "Failed to write time to RTC");
    return;
  }

  ESP_LOGD(TAG, "Time written to RTC: %04d-%02d-%02d %02d:%02d:%02d (DOW: %d)", 
           now.year, now.month, now.day_of_month, 
           now.hour, now.minute, now.second, day_of_week);
}

bool DS3231Component::read_rtc_() {
  // Read all 19 registers
  if (!this->read_bytes(0, (uint8_t*)&ds3231_reg_, sizeof(ds3231_reg_))) {
    ESP_LOGE(TAG, "Can't read I2C data from DS3231");
    return false;
  }

  // Log register state for debugging
  ESP_LOGD(TAG, "DS3231 Registers - "
           "Time: %02X:%02X:%02X "
           "Date: %02X/%02X/20%02X "
           "DOW: %02X "
           "Ctrl: %02X Status: %02X "
           "Temp: %d.%02d°C",
           ds3231_reg_.hours, ds3231_reg_.minutes, ds3231_reg_.seconds,
           ds3231_reg_.day, ds3231_reg_.month, ds3231_reg_.year,
           ds3231_reg_.day_of_week,
           ds3231_reg_.control, ds3231_reg_.status,
           (int8_t)ds3231_reg_.temp_msb,
           ((ds3231_reg_.temp_lsb >> 6) * 25));

  return true;
}

bool DS3231Component::write_rtc_() {
  // Write time/date registers (0x00-0x06)
  if (!this->write_bytes(0, (uint8_t*)&ds3231_reg_, 7)) {
    ESP_LOGE(TAG, "Can't write time/date registers to DS3231");
    return false;
  }

  // Write control register (0x0E)
  if (!this->write_byte(0x0E, ds3231_reg_.control)) {
    ESP_LOGE(TAG, "Can't write control register to DS3231");
    return false;
  }

  // Write status register (0x0F)
  if (!this->write_byte(0x0F, ds3231_reg_.status)) {
    ESP_LOGE(TAG, "Can't write status register to DS3231");
    return false;
  }

  ESP_LOGD(TAG, "Registers written successfully");
  return true;
}

}  // namespace ds3231
}  // namespace esphome
