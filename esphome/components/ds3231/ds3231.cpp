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
  if (ds3231_.reg.osf) {
    ESP_LOGW(TAG, "Oscillator was stopped, clearing status flag");
    ds3231_.reg.osf = false;
    ds3231_.reg.a1f = false;
    ds3231_.reg.a2f = false;
    this->write_rtc_();
  }

  // Enable oscillator (set EOSC=0) and basic configuration
  ds3231_.reg.eosc = false;  // Enable oscillator
  ds3231_.reg.intcn = false; // Square wave output
  ds3231_.reg.a1ie = false;  // Disable alarm interrupts
  ds3231_.reg.a2ie = false;
  ds3231_.reg.en32khz = false; // Disable 32kHz output
  
  this->write_rtc_();

  ESP_LOGD(TAG, "DS3231 initialized successfully");
}

void DS3231Component::update() { 
  this->read_time(); 
  
  // Read temperature if sensor is configured
  if (this->temperature_sensor_ != nullptr && this->read_rtc_()) {
    int8_t temp_whole = ds3231_.reg.temp_msb;
    uint8_t temp_frac = (ds3231_.reg.temp_lsb >> 6) * 25; // 0.25°C steps
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
  uint8_t seconds = ds3231_.reg.seconds + ds3231_.reg.seconds_10 * 10;
  uint8_t minutes = ds3231_.reg.minutes + ds3231_.reg.minutes_10 * 10;
  uint8_t hours = ds3231_.reg.hours + ds3231_.reg.hours_10 * 10;
  uint8_t day_of_month = ds3231_.reg.day + ds3231_.reg.day_10 * 10;
  uint8_t month = ds3231_.reg.month + ds3231_.reg.month_10 * 10;
  uint16_t year = ds3231_.reg.year + ds3231_.reg.year_10 * 10 + 2000;

  // Apply century bit
  if (ds3231_.reg.century) {
    year += 100;  // 2100-2199
  }

  // Handle 12-hour mode
  if (ds3231_.reg.is_12h) {
    hours = hours % 12;
    if (ds3231_.reg.is_pm) {
      hours += 12;
    }
  }

  ESPTime rtc_time{
      .second = seconds,
      .minute = minutes,
      .hour = hours,
      .day_of_week = ds3231_.reg.day_of_week,
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
  // Using Zeller's congruence
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
  ds3231_.reg.seconds = now.second % 10;
  ds3231_.reg.seconds_10 = now.second / 10;
  
  ds3231_.reg.minutes = now.minute % 10;
  ds3231_.reg.minutes_10 = now.minute / 10;
  
  ds3231_.reg.hours = now.hour % 10;
  ds3231_.reg.hours_10 = now.hour / 10;
  ds3231_.reg.is_12h = false;  // Always use 24-hour mode
  ds3231_.reg.is_pm = false;
  
  ds3231_.reg.day_of_week = day_of_week;
  
  ds3231_.reg.day = now.day_of_month % 10;
  ds3231_.reg.day_10 = now.day_of_month / 10;
  
  ds3231_.reg.month = now.month % 10;
  ds3231_.reg.month_10 = now.month / 10;
  
  // Set century bit based on year
  if (now.year >= 2100) {
    ds3231_.reg.century = true;
    ds3231_.reg.year = (now.year - 2100) % 10;
    ds3231_.reg.year_10 = (now.year - 2100) / 10;
  } else {
    ds3231_.reg.century = false;
    ds3231_.reg.year = (now.year - 2000) % 10;
    ds3231_.reg.year_10 = (now.year - 2000) / 10;
  }

  // Clear oscillator stop flag
  ds3231_.reg.osf = false;

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
  if (!this->read_bytes(0, this->ds3231_.raw, sizeof(this->ds3231_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data from DS3231");
    return false;
  }

  // Log register state for debugging
  ESP_LOGD(TAG, "DS3231 Registers - "
           "Time: %u%u:%u%u:%u%u "
           "Date: %u%u/%u%u/20%u%u "
           "DOW: %u "
           "Ctrl: %02X Status: %02X "
           "Temp: %d.%02d°C",
           ds3231_.reg.hours_10, ds3231_.reg.hours,
           ds3231_.reg.minutes_10, ds3231_.reg.minutes, 
           ds3231_.reg.seconds_10, ds3231_.reg.seconds,
           ds3231_.reg.day_10, ds3231_.reg.day,
           ds3231_.reg.month_10, ds3231_.reg.month,
           ds3231_.reg.year_10, ds3231_.reg.year,
           ds3231_.reg.day_of_week,
           *(uint8_t*)&ds3231_.reg.control,
           *(uint8_t*)&ds3231_.reg.status,
           (int8_t)ds3231_.reg.temp_msb,
           ((ds3231_.reg.temp_lsb >> 6) * 25));

  return true;
}

bool DS3231Component::write_rtc_() {
  // Write time/date registers (0x00-0x06)
  uint8_t time_date_registers[7];
  time_date_registers[0] = ((ds3231_.reg.seconds_10 & 0x07) << 4) | (ds3231_.reg.seconds & 0x0F);
  time_date_registers[1] = ((ds3231_.reg.minutes_10 & 0x07) << 4) | (ds3231_.reg.minutes & 0x0F);
  
  // Hours register with 24-hour mode
  uint8_t hour_value = ((ds3231_.reg.hours_10 & 0x03) << 4) | (ds3231_.reg.hours & 0x0F);
  if (!ds3231_.reg.is_12h) {
    hour_value &= 0x3F; // Ensure 24-hour mode
  }
  time_date_registers[2] = hour_value;
  
  time_date_registers[3] = ds3231_.reg.day_of_week & 0x07;
  time_date_registers[4] = ((ds3231_.reg.day_10 & 0x03) << 4) | (ds3231_.reg.day & 0x0F);
  
  // Month register with century bit
  uint8_t month_value = ((ds3231_.reg.month_10 & 0x01) << 4) | (ds3231_.reg.month & 0x0F);
  if (ds3231_.reg.century) {
    month_value |= 0x80;
  }
  time_date_registers[5] = month_value;
  
  time_date_registers[6] = ((ds3231_.reg.year_10 & 0x0F) << 4) | (ds3231_.reg.year & 0x0F);

  if (!this->write_bytes(0, time_date_registers, sizeof(time_date_registers))) {
    ESP_LOGE(TAG, "Can't write time/date registers to DS3231");
    return false;
  }

  // Write control register (0x0E)
  uint8_t control_reg = (ds3231_.reg.rs & 0x03) |
                       (ds3231_.reg.intcn ? 0x04 : 0x00) |
                       (ds3231_.reg.a2ie ? 0x02 : 0x00) |
                       (ds3231_.reg.a1ie ? 0x01 : 0x00);
  // Note: EOSC bit is inverted in the register (0=enable, 1=disable)
  control_reg |= (ds3231_.reg.eosc ? 0x00 : 0x80);

  if (!this->write_byte(0x0E, control_reg)) {
    ESP_LOGE(TAG, "Can't write control register to DS3231");
    return false;
  }

  // Write status register (0x0F)
  uint8_t status_reg = (ds3231_.reg.en32khz ? 0x08 : 0x00) |
                      (ds3231_.reg.bsy ? 0x04 : 0x00) |
                      (ds3231_.reg.a2f ? 0x02 : 0x00) |
                      (ds3231_.reg.a1f ? 0x01 : 0x00) |
                      (ds3231_.reg.osf ? 0x80 : 0x00);

  if (!this->write_byte(0x0F, status_reg)) {
    ESP_LOGE(TAG, "Can't write status register to DS3231");
    return false;
  }

  ESP_LOGD(TAG, "Registers written successfully");
  return true;
}

}  // namespace ds3231
}  // namespace esphome
