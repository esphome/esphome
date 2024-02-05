#include "ds3232.h"
#include "ds3232_alarm.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://www.analog.com/media/en/technical-documentation/data-sheets/ds3232m.pdf

namespace esphome {
namespace ds3232 {

namespace ds3232alarms {}  // namespace ds3232alarms

static const char *const TAG = "ds3232";

static const char *const DOW_TEXT = "day of week";
static const char *const DOM_TEXT = "day of month";

/// Factor to calculate temperature of DS3232
static const float TEMPERATURE_FACTOR = 0.25;

static const uint8_t I2C_REG_RTC = 0x00;
static const uint8_t I2C_REG_ALARM_1 = 0x07;
static const uint8_t I2C_REG_ALARM_2 = 0x0B;
static const uint8_t I2C_REG_CONTROL = 0x0E;
static const uint8_t I2C_REG_STATUS = 0x0F;
static const uint8_t I2C_REG_AGING = 0x10;
static const uint8_t I2C_REG_TEMPERATURE = 0x11;
static const uint8_t I2C_REG_START = I2C_REG_RTC;

void DS3232Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS3232...");
  this->power_state_.next(DS3232PowerState::UNKNOWN);

  if (!this->read_data_()) {
    ESP_LOGE(TAG, "Unable to read data from crystal.");
    this->mark_failed();
  } else {
    ESP_LOGV(TAG, "Checking for oscillator state.");
    this->reinit_osf_();
  }

  this->nvram_available_ = this->read_bytes(SVC_NVRAM_ADDRESS, this->nvram_info_.raw, sizeof(this->nvram_info_.raw));

  if (!this->nvram_available_) {
    ESP_LOGE(TAG, "NVRAM: Unable to access NVRAM. Marking NVRAM functionality as failed.");
    this->nvram_state_ = DS3232NVRAMState::FAIL;
    this->variable_fail_callback_.call();
  } else {
    if ((this->nvram_info_.info.magix_1 == ds3232::MAGIX_CONTROL_1) &&
        (this->nvram_info_.info.magix_2 == ds3232::MAGIX_CONTROL_2)) {
      ESP_LOGI(TAG, "NVRAM usage has been detected.");
      if (!this->nvram_info_.info.is_initialized) {
        ESP_LOGI(TAG, "NVRAM will be written with initial values.");
        this->plan_reset_nvram_();
      } else {
        ESP_LOGI(TAG, "Found NVRAM. Data version: %.2u.%.1u", this->nvram_info_.info.maj_version,
                 this->nvram_info_.info.min_version);
        if ((this->nvram_info_.info.maj_version * 10 + this->nvram_info_.info.min_version) >
            ds3232::NVRAM_DATA_VERSION) {
          ESP_LOGW(TAG,
                   "Supported version (%.2u.%.1u) lower than stored version. Behaviour could be unpredictable. "
                   "Consider to reset NVRAM.",
                   ds3232::NVRAM_DATA_MAJ_VERSION, ds3232::NVRAM_DATA_MIN_VERSION);
        }
        this->nvram_state_ = DS3232NVRAMState::OK;
      }
    } else {
      ESP_LOGI(TAG,
               "Non-complaint data in NVRAM has been detected. NVRAM will be initialized. This will take some time.");
      this->plan_clear_nvram_();
    };
    ESP_LOGD(TAG, "DS3232 NVRAM initialization complete");
    this->late_startup_ = false;
  }

  // Workaround for states when alarms fired when ESPHome device was unpowered but DS3232 was online.
  // There is a problem with A1F / A2F registers - INT signal will not be issued when this registers already set to 1.
  // So there is a need to reset them to 0 on startup.
  if (this->reg_data_.reg.alarm_1_match || this->reg_data_.reg.alarm_2_match) {
    if (this->fire_alarms_on_startup_) {
      process_alarms_();
    } else {
      reg_data_.reg.alarm_1_match = false;
      reg_data_.reg.alarm_2_match = false;
      this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
    }
  }
}

void DS3232Component::update() {
  if (this->late_startup_)
    return;
  this->read_time();
  this->read_temperature_();
}

// Need to initialize afted i2c bus.
// I don't know why but on my ESP32 board i2c bus initialized and
// ready for connection too late. As a result sometimes
// component marked as failed.
float DS3232Component::get_setup_priority() const { return setup_priority::DATA - 200; }

void DS3232Component::reinit_osf_() {
  this->read_data_();
  if (!this->reg_data_.reg.osf_bit)
    return;
  ESP_LOGD(TAG, "Found disabled oscillator. Restarting it.");
  this->reg_data_.reg.osf_bit = false;
  if (!this->write_byte(I2C_REG_STATUS, this->reg_data_.raw_blocks.status_raw[0])) {
    ESP_LOGE(TAG, "Unable to restart oscillator.");
    return;
  }
}

void DS3232Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3232");
  LOG_PIN("  Interrupt / Heartbeat pin: ", this->int_pin_);
  LOG_PIN("  Reset / Power pin: ", this->rst_pin_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS3232 failed!");
  } else {
    this->read_data_();
    ESP_LOGCONFIG(TAG, " Timezone: '%s'", this->timezone_.c_str());
    ESP_LOGCONFIG(TAG, " Internal config from chip:");
    ESP_LOGCONFIG(TAG, "  Ocsillation active: %s", TRUEFALSE(!this->reg_data_.reg.osf_bit));
    ESP_LOGCONFIG(TAG, "  Heartbeat mode: %s", ONOFF(!this->reg_data_.reg.int_enable));
    ESP_LOGCONFIG(TAG, "  Alarms mode: %s", ONOFF(this->reg_data_.reg.int_enable));
    ds3232_alarm::DS3232Alarm alarm;
    alarm = this->get_alarm_one();
    if (alarm.enabled) {
      ESP_LOGCONFIG(TAG, "  Alarm 1: %s", this->reg_data_.reg.int_enable ? "ACTIVE" : "SUSPENDED");
      ESP_LOGCONFIG(TAG, "   Start on %s", alarm.to_string().c_str());
      ESP_LOGCONFIG(TAG, "   Matched: %s", TRUEFALSE(this->reg_data_.reg.alarm_1_match));
    } else {
      ESP_LOGCONFIG(TAG, "  Alarm 1: INACTIVE");
    };
    alarm = this->get_alarm_two();
    if (alarm.enabled) {
      ESP_LOGCONFIG(TAG, "  Alarm 2: %s", this->reg_data_.reg.int_enable ? "ACTIVE" : "SUSPENDED");
      ESP_LOGCONFIG(TAG, "   Start on %s", alarm.to_string().c_str());
      ESP_LOGCONFIG(TAG, "   Matched: %s", TRUEFALSE(this->reg_data_.reg.alarm_2_match));
    } else {
      ESP_LOGCONFIG(TAG, "  Alarm 2: INACTIVE");
    }
  }
}

void DS3232Component::loop() {
  auto pow_pin_state = DS3232PowerState::UNKNOWN;
  if (this->rst_pin_ != nullptr) {
    pow_pin_state = this->rst_pin_->digital_read() ? DS3232PowerState::ONLINE : DS3232PowerState::BATTERY;
  }
  if (this->power_state_.next(pow_pin_state)) {
    ESP_LOGI(TAG, "Power source changed: %u", pow_pin_state);
    this->power_state_callback_.call(pow_pin_state);
  }
  if (this->int_pin_ != nullptr) {
    auto pin_state = this->int_pin_->digital_read();
    if ((this->pin_state_ != pin_state) && !pin_state) {
      ESP_LOGD(TAG, "Got active low on INT pin.");
      this->process_interrupt_();
    }
    this->pin_state_ = pin_state;
  }
  switch (this->nvram_state_) {
    case DS3232NVRAMState::NEED_INITIALIZATION:
      if (this->busy_) {
        ESP_LOGI(TAG, "Performing planned NVRAM full reset...");
        clear_nvram_();
      }
      break;
    case DS3232NVRAMState::NEED_RESET:
      if (this->busy_) {
        ESP_LOGI(TAG, "Performing planned NVRAM factory reset...");
        this->nvram_state_ = DS3232NVRAMState::OK;
        this->variable_init_callback_.call();
        if (this->nvram_state_ != DS3232NVRAMState::OK) {
          ESP_LOGE(TAG, "NVRAM: Failed to reset to factory defaults.");
        }
        this->busy_ = false;
      }
      break;
    default:
      break;
  }
}

void DS3232Component::plan_clear_nvram_() {
  this->busy_ = true;
  this->nvram_state_ = DS3232NVRAMState::NEED_INITIALIZATION;
}

void DS3232Component::plan_reset_nvram_() {
  this->busy_ = true;
  this->nvram_state_ = DS3232NVRAMState::NEED_RESET;
}

void DS3232Component::clear_nvram_() {
  const uint8_t write_len = 8;

  this->nvram_state_ = DS3232NVRAMState::INITIALIZATION;
  ESP_LOGD(TAG, "NVRAM: Begin to clear memory.");
  uint8_t total_size = MAX_NVRAM_ADDRESS - MIN_NVRAM_ADDRESS + 1;
  ESP_LOGD(TAG, "NVRAM: Total size to clear: %.2d byte(s)", total_size);
  uint8_t step = 0;
  bool state = true;
  std::vector<uint8_t> zeroes;
  zeroes.resize(write_len, 0);

  while (step < total_size) {
    bool res = false;
    if ((total_size - step) < write_len) {
      res = this->write_bytes(MIN_NVRAM_ADDRESS + step, zeroes.data(), total_size - step);
      step += (total_size - step);
    } else {
      res = this->write_bytes(MIN_NVRAM_ADDRESS + step, zeroes);
      step += write_len;
    }
    ESP_LOGV(TAG, "NVRAM: Trying to set %#02x register to 0x00: %s", MIN_NVRAM_ADDRESS + step, YESNO(res));
    state &= res;
  }

  if (!state) {
    ESP_LOGE(TAG, "NVRAM: Unable to clear memory. Marking as failed.");
    this->nvram_state_ = DS3232NVRAMState::FAIL;
  } else {
    ESP_LOGD(TAG, "NVRAM: Memory has been cleared. Writing service information.");
    this->nvram_info_.info.magix_1 = MAGIX_CONTROL_1;
    this->nvram_info_.info.magix_2 = MAGIX_CONTROL_2;
    this->nvram_info_.info.is_initialized = false;
    this->nvram_info_.info.maj_version = NVRAM_DATA_MAJ_VERSION;
    this->nvram_info_.info.min_version = NVRAM_DATA_MIN_VERSION;
    if (!this->write_bytes(SVC_NVRAM_ADDRESS, this->nvram_info_.raw, sizeof(this->nvram_info_.raw))) {
      ESP_LOGE(TAG, "NVRAM: Unable to write service NVRAM information. Marking as failed.");
      this->nvram_state_ = DS3232NVRAMState::FAIL;
    }
    ESP_LOGD(TAG, "NVRAM: Initializing variables with initial values.");
    this->variable_init_callback_.call();
    ESP_LOGD(TAG, "NVRAM: Variables has been initialized. Saving state.");

    this->nvram_info_.info.is_initialized = true;
    if (!this->write_bytes(SVC_NVRAM_ADDRESS, this->nvram_info_.raw, sizeof(this->nvram_info_.raw))) {
      ESP_LOGE(TAG, "NVRAM: Unable to write service NVRAM information. Marking as failed.");
      this->nvram_state_ = DS3232NVRAMState::FAIL;
    }
    this->nvram_state_ = DS3232NVRAMState::OK;
    ESP_LOGI(TAG, "NVRAM has been cleared.");
  }
  this->busy_ = false;
}

bool DS3232Component::validate_mem_(uint8_t reg_id, uint8_t size, bool ignore_empty) {
  bool validator = true;
  validator &= (ignore_empty || this->nvram_available_);
  validator &= reg_id >= ds3232::MIN_NVRAM_ADDRESS;
  validator &= (reg_id + size - 1) <= ds3232::MAX_NVRAM_ADDRESS;
  return validator;
}

void DS3232Component::reset_memory() {
  if (this->nvram_state_ == DS3232NVRAMState::INITIALIZATION) {
    ESP_LOGW(TAG, "NVRAM: Another memory reset process in progress already.");
    return;
  }
  ESP_LOGI(TAG, "Resetting NVRAM has been requested.");
  this->plan_clear_nvram_();
}

void DS3232Component::reset_to_factory() {
  if (this->nvram_state_ == DS3232NVRAMState::INITIALIZATION) {
    ESP_LOGW(TAG, "NVRAM: Another memory reset process in progress already.");
    return;
  }
  ESP_LOGI(TAG, "Resetting config variables to their initial values.");
  this->plan_reset_nvram_();
}

bool DS3232Component::read_memory(uint8_t reg_id, std::vector<uint8_t> &data) {
  if (this->nvram_state_ == DS3232NVRAMState::INITIALIZATION) {
    ESP_LOGW(TAG, "NVRAM: Memory reset process in progress. Try later.");
    return false;
  }
  if (data.size() == 0) {
    ESP_LOGW(TAG, "NVRAM: Nothing to write to memory.");
    return true;
  }
  if (!this->validate_mem_(reg_id, data.size())) {
    ESP_LOGE(TAG, "Invalid NVRAM memory mapping.");
    return false;
  } else {
    if (!this->read_bytes(reg_id, data.data(), data.size())) {
      ESP_LOGE(TAG, "NVRAM: Unable to read from %#02x register with size %u.", reg_id, data.size());
      this->nvram_state_ = DS3232NVRAMState::FAIL;
      return false;
    };
    ESP_LOGD(TAG, "NVRAM: Value read from %#02x register with size %u.", reg_id, data.size());
    return true;
  }
}

bool DS3232Component::write_memory(const uint8_t reg_id, const std::vector<uint8_t> &data) {
  if (data.size() == 0) {
    ESP_LOGW(TAG, "NVRAM: Nothing to write to memory.");
    return true;
  }

  if (this->nvram_state_ == DS3232NVRAMState::INITIALIZATION) {
    ESP_LOGW(TAG, "NVRAM: Memory reset process in progress. Try later.");
    return false;
  }

  if (!this->validate_mem_(reg_id, data.size(), true)) {
    ESP_LOGE(TAG, "Invalid NVRAM memory mapping.");
    return false;
  }

  if (!this->write_bytes(reg_id, data)) {
    ESP_LOGE(TAG, "NVRAM: Unable to write to %#02x register with size %u.", reg_id, data.size());
    this->nvram_state_ = DS3232NVRAMState::FAIL;
    return false;
  } else {
    ESP_LOGD(TAG, "NVRAM: Value written to %#02x register with size %u.", reg_id, data.size());
    return true;
  }
}

void DS3232Component::process_interrupt_() {
  if (this->read_data_()) {
    if (!reg_data_.reg.int_enable) {
      ESP_LOGD(TAG, "1Hz Heartbeat fired.");
      this->heartbeat_callback_.call();
    } else {
      process_alarms_();
    }
  }
}

void DS3232Component::process_alarms_() {
  auto firing_time = this->reg_to_time_();
  if (reg_data_.reg.alarm_1_enable && reg_data_.reg.alarm_1_match) {
    reg_data_.reg.alarm_1_match = false;
    ESP_LOGI(TAG, "Alarm 1 has been fired.");
    this->alarm_one_callback_.call(firing_time);
  };
  if (reg_data_.reg.alarm_2_enable && reg_data_.reg.alarm_2_match) {
    reg_data_.reg.alarm_2_match = false;
    ESP_LOGI(TAG, "Alarm 2 has been fired.");
    this->alarm_two_callback_.call(firing_time);
  };
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
}

/// @brief Reads i2c registers from chip
/// @return True - if data has been read; otherwise - false
bool DS3232Component::read_data_() {
  if (!this->read_bytes(I2C_REG_START, this->reg_data_.raw, sizeof(this->reg_data_.raw))) {
    ESP_LOGE(TAG, "Unable to read I2C data.");
    return false;
  }

  ESP_LOGV(TAG,
           "Read registers:\nRTC: %0u%0u:%0u%0u:%0u%0u 2%0u%0u%0u-%0u%0u-%0u%0u\nAlarm 1 (%s): 0b%0u%0u%0u%0u "
           "%0u%0u:%0u%0u:%0u%0u / %0u%0u (as %s)\nAlarm 2 (%s): 0b%0u%0u%0u0 %0u%0u:%0u%0u:00 / %0u%0u (as "
           "%s)\nTemperature: (%d + 0.25*%0u) °C\nEOSC:%s BBSQW:%s CONV:%s INTCN:%s\nOSCF:%s BB32KHZ:%s EN32KHZ:%s "
           "BSY:%s A2F:%0u A1F:%0u",
           reg_data_.reg.hour_10, reg_data_.reg.hour, reg_data_.reg.minute_10, reg_data_.reg.minute,
           reg_data_.reg.second_10, reg_data_.reg.second, reg_data_.reg.century, reg_data_.reg.year_10,
           reg_data_.reg.year, reg_data_.reg.month_10, reg_data_.reg.month, reg_data_.reg.day_10, reg_data_.reg.day,
           ONOFF(reg_data_.reg.alarm_1_enable), reg_data_.reg.alarm_1_mode_4, reg_data_.reg.alarm_1_mode_3,
           reg_data_.reg.alarm_1_mode_2, reg_data_.reg.alarm_1_mode_1, reg_data_.reg.alarm_1_hour_10,
           reg_data_.reg.alarm_1_hour, reg_data_.reg.alarm_1_minute_10, reg_data_.reg.alarm_1_minute,
           reg_data_.reg.alarm_1_second_10, reg_data_.reg.alarm_1_second, reg_data_.reg.alarm_1_day_10,
           reg_data_.reg.alarm_1_day, (reg_data_.reg.alarm_1_use_day_of_week ? DOW_TEXT : DOM_TEXT),
           ONOFF(reg_data_.reg.alarm_2_enable), reg_data_.reg.alarm_2_mode_4, reg_data_.reg.alarm_2_mode_3,
           reg_data_.reg.alarm_2_mode_2, reg_data_.reg.alarm_2_hour_10, reg_data_.reg.alarm_2_hour,
           reg_data_.reg.alarm_2_minute_10, reg_data_.reg.alarm_2_minute, reg_data_.reg.alarm_2_day_10,
           reg_data_.reg.alarm_2_day, (reg_data_.reg.alarm_2_use_day_of_week ? DOW_TEXT : DOM_TEXT),
           reg_data_.reg.temperature_integral, reg_data_.reg.temperature_fractional, ONOFF(reg_data_.reg.EOSC),
           ONOFF(reg_data_.reg.enable_battery_square_signal), ONOFF(reg_data_.reg.convert_temp),
           ONOFF(reg_data_.reg.int_enable), ONOFF(reg_data_.reg.osf_bit),
           ONOFF(reg_data_.reg.enable_hf_output_on_battery_mode), ONOFF(reg_data_.reg.enable_hf_output),
           ONOFF(reg_data_.reg.device_busy), reg_data_.reg.alarm_2_match, reg_data_.reg.alarm_1_match);
  return true;
}

/// @brief Reads temperature from internal chip sensor and updates sensor component on data change
void DS3232Component::read_temperature_() {
  float temperature = reg_data_.reg.temperature_integral + TEMPERATURE_FACTOR * reg_data_.reg.temperature_fractional;
  ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
  if (this->temperature_sensor_ != nullptr) {
    if (this->temperature_state_.next(temperature)) {
      this->temperature_sensor_->publish_state(temperature);
    };
  };
}

/// @brief Creates ESPTime struct from registers data
/// @return ESPTime struct that contains time stored in RTC registers.
ESPTime DS3232Component::reg_to_time_() {
  return ESPTime{
      .second = uint8_t(reg_data_.reg.second + 10 * reg_data_.reg.second_10),
      .minute = uint8_t(reg_data_.reg.minute + 10u * reg_data_.reg.minute_10),
      .hour = uint8_t(reg_data_.reg.hour + 10u * reg_data_.reg.hour_10),
      .day_of_week = uint8_t(reg_data_.reg.weekday),
      .day_of_month = uint8_t(reg_data_.reg.day + 10u * reg_data_.reg.day_10),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = uint8_t(reg_data_.reg.month + 10u * reg_data_.reg.month_10),
      .year = uint16_t(reg_data_.reg.year + 10u * reg_data_.reg.year_10 + 100u * reg_data_.reg.century + 2000)};
}

/// @brief Reads time from chip
void DS3232Component::read_time() {
  if (!this->read_data_()) {
    return;
  }
  if (reg_data_.reg.osf_bit) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  if (reg_data_.reg.device_busy) {
    ESP_LOGW(TAG, "RTC busy, not syncing to system clock.");
    return;
  }
  ESPTime current_rtc_time = this->reg_to_time_();
  current_rtc_time.recalc_timestamp_utc(false);
  if (!current_rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(current_rtc_time.timestamp);
}

/// @brief Writes current time to chip
void DS3232Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  reg_data_.reg.year = (now.year >= 2100) ? ((now.year - 2100) % 10) : ((now.year - 2000) % 10);
  reg_data_.reg.year_10 = ((now.year >= 2100) ? ((now.year - 2100) / 10) : ((now.year - 2000) / 10)) % 10;
  reg_data_.reg.century = (now.year >= 2100) ? 1 : 0;
  reg_data_.reg.month = now.month % 10;
  reg_data_.reg.month_10 = now.month / 10;
  reg_data_.reg.day = now.day_of_month % 10;
  reg_data_.reg.day_10 = now.day_of_month / 10;
  reg_data_.reg.weekday = now.day_of_week;
  reg_data_.reg.hour = now.hour % 10;
  reg_data_.reg.hour_10 = now.hour / 10;
  reg_data_.reg.minute = now.minute % 10;
  reg_data_.reg.minute_10 = now.minute / 10;
  reg_data_.reg.second = now.second % 10;
  reg_data_.reg.second_10 = now.second / 10;
  reg_data_.reg.osf_bit = true;
  reg_data_.reg.EOSC = false;

  this->write_bytes(I2C_REG_RTC, reg_data_.raw_blocks.rtc_raw, sizeof(reg_data_.raw_blocks.rtc_raw));
  this->write_bytes(I2C_REG_CONTROL, reg_data_.raw_blocks.control_raw, sizeof(reg_data_.raw_blocks.control_raw));
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
}

/// @brief Gets alarm one state from chip
/// @return A DS3232Alarm struct that contains detailed information about alarm
ds3232_alarm::DS3232Alarm DS3232Component::get_alarm_one() {
  if (!this->read_data_()) {
    return {};
  }
  ds3232_alarm::DS3232Alarm alarm = {};
  alarm.enabled = reg_data_.reg.alarm_1_enable;
  alarm.seconds_supported = true;
  alarm.mode = ds3232_alarm::DS3232Alarm::alarm_mode(reg_data_.reg.alarm_1_mode_1, reg_data_.reg.alarm_1_mode_2,
                                                     reg_data_.reg.alarm_1_mode_3, reg_data_.reg.alarm_1_mode_4,
                                                     reg_data_.reg.alarm_1_use_day_of_week);
  alarm.second = reg_data_.reg.alarm_1_second + 10u * reg_data_.reg.alarm_1_second_10;
  alarm.minute = reg_data_.reg.alarm_1_minute + 10u * reg_data_.reg.alarm_1_minute_10;
  alarm.hour = reg_data_.reg.alarm_1_hour + 10u * reg_data_.reg.alarm_1_hour_10;
  alarm.day_of_week = reg_data_.reg.alarm_1_day;
  alarm.day_of_month = reg_data_.reg.alarm_1_day + 10u * reg_data_.reg.alarm_1_day_10;
  alarm.is_fired = reg_data_.reg.alarm_1_match;
  if (!alarm.is_valid()) {
    ESP_LOGW(TAG, "Stored 1st alarm data is invalid.");
  }
  return alarm;
}

/// @brief Gets alarm two state from chip
/// @return A DS3232Alarm struct that contains detailed information about alarm
ds3232_alarm::DS3232Alarm DS3232Component::get_alarm_two() {
  if (!this->read_data_()) {
    return {};
  }
  ds3232_alarm::DS3232Alarm alarm = {};
  alarm.enabled = reg_data_.reg.alarm_2_enable;
  alarm.seconds_supported = false;
  alarm.mode =
      ds3232_alarm::DS3232Alarm::alarm_mode(false, reg_data_.reg.alarm_2_mode_2, reg_data_.reg.alarm_2_mode_3,
                                            reg_data_.reg.alarm_2_mode_4, reg_data_.reg.alarm_2_use_day_of_week);
  alarm.second = 0;
  alarm.minute = reg_data_.reg.alarm_2_minute + 10u * reg_data_.reg.alarm_2_minute_10;
  alarm.hour = reg_data_.reg.alarm_2_hour + 10u * reg_data_.reg.alarm_2_hour_10;
  alarm.day_of_week = reg_data_.reg.alarm_2_day;
  alarm.day_of_month = reg_data_.reg.alarm_2_day + 10u * reg_data_.reg.alarm_2_day_10;
  alarm.is_fired = reg_data_.reg.alarm_2_match;
  if (!alarm.is_valid()) {
    ESP_LOGW(TAG, "Stored 2nd alarm data is invalid.");
  }
  return alarm;
}

/// @brief Resets alarm one data to initial settings and disables it.
void DS3232Component::clear_alarm_one() {
  ds3232_alarm::DS3232Alarm alarm = this->get_alarm_one();
  alarm.enabled = false;
  alarm.second = 0;
  alarm.minute = 0;
  alarm.hour = 0;
  alarm.day_of_month = 1;
  alarm.day_of_week = 1;
  alarm.mode = ds3232_alarm::DS3232Alarm::alarm_mode(ds3232_alarm::AlarmMode::EVERY_TIME);
  this->set_alarm_one(alarm);
}

/// @brief Resets alarm two data to initial settings and disables it.
void DS3232Component::clear_alarm_two() {
  ds3232_alarm::DS3232Alarm alarm = this->get_alarm_two();
  alarm.enabled = false;
  alarm.second = 0;
  alarm.minute = 0;
  alarm.hour = 0;
  alarm.day_of_month = 1;
  alarm.day_of_week = 1;
  alarm.mode = ds3232_alarm::DS3232Alarm::alarm_mode(ds3232_alarm::AlarmMode::EVERY_TIME);
  this->set_alarm_two(alarm);
}

/// @brief Sets the 1st alarm
/// @param alarm Struct that contains detailed information about alarm to set.
void DS3232Component::set_alarm_one(const ds3232_alarm::DS3232Alarm &alarm) {
  if (!alarm.is_valid()) {
    ESP_LOGW(TAG, "Unable to set alarm one to invalid state.");
    return;
  }
  if (!this->read_data_()) {
    return;
  }
  reg_data_.reg.alarm_1_enable = alarm.enabled;
  reg_data_.reg.alarm_1_second = alarm.second % 10;
  reg_data_.reg.alarm_1_second_10 = alarm.second / 10;
  reg_data_.reg.alarm_1_minute = alarm.minute % 10;
  reg_data_.reg.alarm_1_minute_10 = alarm.minute / 10;
  reg_data_.reg.alarm_1_hour = alarm.hour % 10;
  reg_data_.reg.alarm_1_hour_10 = alarm.hour / 10;
  reg_data_.reg.alarm_1_day = alarm.mode.bits.use_weekdays ? alarm.day_of_week : alarm.day_of_month % 10;
  reg_data_.reg.alarm_1_day_10 = alarm.mode.bits.use_weekdays ? 0 : alarm.day_of_month / 10;
  reg_data_.reg.alarm_1_mode_1 = alarm.mode.bits.match_seconds;
  reg_data_.reg.alarm_1_mode_2 = alarm.mode.bits.match_minutes;
  reg_data_.reg.alarm_1_mode_3 = alarm.mode.bits.match_hours;
  reg_data_.reg.alarm_1_mode_4 = alarm.mode.bits.match_days;
  reg_data_.reg.alarm_1_use_day_of_week = alarm.mode.bits.use_weekdays;
  reg_data_.reg.alarm_2_match = false;
  reg_data_.reg.int_enable =
      (reg_data_.reg.alarm_1_enable || reg_data_.reg.alarm_2_enable) && !this->produce_stable_frequency_;

  if (!this->write_bytes(I2C_REG_ALARM_1, reg_data_.raw_blocks.alarm_1_raw, sizeof(reg_data_.raw_blocks.alarm_1_raw))) {
    return;
  }
  this->write_bytes(I2C_REG_CONTROL, reg_data_.raw_blocks.control_raw, sizeof(reg_data_.raw_blocks.control_raw));
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
}

/// @brief Sets the 2nd alarm
/// @param alarm Struct that contains detailed information about alarm to set.
void DS3232Component::set_alarm_two(const ds3232_alarm::DS3232Alarm &alarm) {
  if (!alarm.is_valid()) {
    ESP_LOGW(TAG, "Unable to set alarm one to invalid state.");
    return;
  }
  if (!this->read_data_()) {
    return;
  }
  reg_data_.reg.alarm_2_enable = alarm.enabled;
  reg_data_.reg.alarm_2_minute = alarm.minute % 10;
  reg_data_.reg.alarm_2_minute_10 = alarm.minute / 10;
  reg_data_.reg.alarm_2_hour = alarm.hour % 10;
  reg_data_.reg.alarm_2_hour_10 = alarm.hour / 10;
  reg_data_.reg.alarm_2_day = alarm.mode.bits.use_weekdays ? alarm.day_of_week : alarm.day_of_month % 10;
  reg_data_.reg.alarm_2_day_10 = alarm.mode.bits.use_weekdays ? 0 : alarm.day_of_month / 10;
  // reg_data_.reg.alarm_2_mode_1 = alarm.mode.bits.match_seconds; - there is no mode_1 flag for alarm 2.
  reg_data_.reg.alarm_2_mode_2 = alarm.mode.bits.match_minutes;
  reg_data_.reg.alarm_2_mode_3 = alarm.mode.bits.match_hours;
  reg_data_.reg.alarm_2_mode_4 = alarm.mode.bits.match_days;
  reg_data_.reg.alarm_2_use_day_of_week = alarm.mode.bits.use_weekdays;
  reg_data_.reg.alarm_2_match = false;
  reg_data_.reg.int_enable =
      (reg_data_.reg.alarm_1_enable || reg_data_.reg.alarm_2_enable) && !this->produce_stable_frequency_;

  if (!this->write_bytes(I2C_REG_ALARM_2, reg_data_.raw_blocks.alarm_2_raw, sizeof(reg_data_.raw_blocks.alarm_2_raw))) {
    return;
  }
  this->write_bytes(I2C_REG_CONTROL, reg_data_.raw_blocks.control_raw, sizeof(reg_data_.raw_blocks.control_raw));
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
}

/// @brief Gets heartbeat state
/// @return If true then 1Hz signal output on INT pin enabled.
/// If false then INT pin goes low on alarm firing event.
bool DS3232Component::is_heartbeat_enabled() {
  if (!this->read_data_()) {
    return false;
  }
  return !reg_data_.reg.int_enable;
}

/// @brief Enables 1Hz output and disables alarms.
void DS3232Component::enable_heartbeat() {
  if (!this->read_data_()) {
    return;
  }
  reg_data_.reg.int_enable = false;
  reg_data_.reg.alarm_1_match = false;
  reg_data_.reg.alarm_2_match = false;
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
  this->write_bytes(I2C_REG_CONTROL, reg_data_.raw_blocks.control_raw, sizeof(reg_data_.raw_blocks.control_raw));
  this->produce_stable_frequency_ = true;
}

/// @brief Enables alarms functionality and disables 1Hz output.
void DS3232Component::enable_alarms() {
  if (!this->read_data_()) {
    return;
  }
  reg_data_.reg.int_enable = true;
  reg_data_.reg.alarm_1_match = false;
  reg_data_.reg.alarm_2_match = false;
  this->write_bytes(I2C_REG_STATUS, reg_data_.raw_blocks.status_raw, sizeof(reg_data_.raw_blocks.status_raw));
  this->write_bytes(I2C_REG_CONTROL, reg_data_.raw_blocks.control_raw, sizeof(reg_data_.raw_blocks.control_raw));
  this->produce_stable_frequency_ = false;
}

}  // namespace ds3232
}  // namespace esphome
