#include "ds3231.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf

namespace esphome::ds3231 {

ESPHOME_LOG_TAG(TAG, "ds3231");

static constexpr uint8_t DS3231_REG_SECONDS = 0x00;
static constexpr uint8_t DS3231_REG_ALARM_1 = 0x07;
static constexpr uint8_t DS3231_REG_ALARM_2 = 0x0B;
static constexpr uint8_t DS3231_REG_CONTROL = 0x0E;
static constexpr uint8_t DS3231_REG_STATUS = 0x0F;
static constexpr uint8_t DS3231_REG_AGING_OFFSET = 0x10;
static constexpr uint8_t DS3231_REG_TEMPERATURE = 0x11;

static constexpr uint8_t HOUR_12H = 0x40;
static constexpr uint8_t HOUR_PM = 0x20;
static constexpr uint8_t MONTH_CENTURY = 0x80;
static constexpr uint8_t ALARM_MASK_BIT = 0x80;     // AxMy: this register is ignored when matching
static constexpr uint8_t ALARM_DAY_OF_WEEK = 0x40;  // DY/DT: match day of week instead of date

/// How long an alarm binary sensor stays on when the flag is cleared automatically.
static constexpr uint32_t ALARM_PULSE_MS = 1000;

static const LogString *alarm_state_to_string(uint8_t control, uint8_t alarm) {
  return (control & (alarm == 1 ? 0x01 : 0x02)) != 0 ? LOG_STR("enabled") : LOG_STR("disabled");
}

static const LogString *square_wave_to_string(SquareWaveFrequency frequency) {
  switch (frequency) {
    case SQUARE_WAVE_1HZ:
      return LOG_STR("1 Hz");
    case SQUARE_WAVE_1024HZ:
      return LOG_STR("1.024 kHz");
    case SQUARE_WAVE_4096HZ:
      return LOG_STR("4.096 kHz");
    case SQUARE_WAVE_8192HZ:
      return LOG_STR("8.192 kHz");
    default:
      return LOG_STR("unknown");
  }
}

void IRAM_ATTR DS3231Component::interrupt_handler(DS3231Component *arg) { arg->enable_loop_soon_any_context(); }

void DS3231Component::setup() {
  if (!this->read_control_status_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->control_reg_ &= ~CONTROL_EOSC;  // keep the oscillator running on battery
  if (this->square_wave_configured_) {
    this->control_reg_ &= ~(CONTROL_INTCN | CONTROL_RS_MASK);
    this->control_reg_ |= (this->square_wave_frequency_ << CONTROL_RS_SHIFT) & CONTROL_RS_MASK;
  } else {
    // INT/SQW is the alarm interrupt line rather than a square-wave output.
    this->control_reg_ |= CONTROL_INTCN;
  }
  if (this->battery_backed_square_wave_ >= 0) {
    if (this->battery_backed_square_wave_ != 0) {
      this->control_reg_ |= CONTROL_BBSQW;
    } else {
      this->control_reg_ &= ~CONTROL_BBSQW;
    }
  }
  // The alarm interrupt enables (A1IE/A2IE) are kept as they are: the alarms live on the chip and
  // survive a reboot of the ESP on battery.
  if (!this->write_control_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if (this->output_32khz_ >= 0) {
    if (this->output_32khz_ != 0) {
      this->status_reg_ |= STATUS_EN32KHZ;
    } else {
      this->status_reg_ &= ~STATUS_EN32KHZ;
    }
    if (!this->write_status_(0)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
  }

  if (this->aging_offset_configured_ &&
      !this->write_byte(DS3231_REG_AGING_OFFSET, static_cast<uint8_t>(this->aging_offset_))) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if ((this->status_reg_ & STATUS_OSF) != 0) {
    ESP_LOGW(TAG, "Oscillator was stopped (battery empty or missing?), the stored time is invalid.");
  }
  this->publish_oscillator_stopped_();

  if (!this->monitor_alarms_) {
    this->disable_loop();
    return;
  }

  // Start from a known state; an alarm that fired while the ESP was off is picked up by the first
  // check, which runs from the loop right after setup.
  this->publish_alarm_state_(1, false);
  this->publish_alarm_state_(2, false);

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    // INT/SQW is open drain and active low: a falling edge means an enabled alarm matched.
    // The handler only wakes the loop; the flags are read from there.
    this->interrupt_pin_->attach_interrupt(&DS3231Component::interrupt_handler, this, gpio::INTERRUPT_FALLING_EDGE);
  }
  // Without an interrupt pin this is how alarms are seen. With one it is a fallback: it picks up an
  // edge that was missed, and latched flags (auto_clear_alarms: false) that were cleared elsewhere.
  this->set_interval(this->alarm_check_interval_, [this]() { this->check_alarms_(); });
}

void DS3231Component::loop() {
  // Only runs once after setup and when woken by the interrupt pin.
  this->disable_loop();
  this->check_alarms_();
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    return;
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  if ((this->control_reg_ & CONTROL_INTCN) == 0) {
    ESP_LOGCONFIG(TAG, "  INT/SQW: square wave %s",
                  LOG_STR_ARG(square_wave_to_string(
                      static_cast<SquareWaveFrequency>((this->control_reg_ & CONTROL_RS_MASK) >> CONTROL_RS_SHIFT))));
  } else {
    ESP_LOGCONFIG(TAG, "  INT/SQW: alarm interrupt");
  }
  ESP_LOGCONFIG(TAG,
                "  Battery-backed square wave: %s\n"
                "  32 kHz output: %s\n"
                "  Alarm 1: %s\n"
                "  Alarm 2: %s\n"
                "  Auto clear alarms: %s",
                YESNO(this->control_reg_ & CONTROL_BBSQW), ONOFF(this->status_reg_ & STATUS_EN32KHZ),
                LOG_STR_ARG(alarm_state_to_string(this->control_reg_, 1)),
                LOG_STR_ARG(alarm_state_to_string(this->control_reg_, 2)), YESNO(this->auto_clear_alarms_));
  if (this->aging_offset_configured_) {
    ESP_LOGCONFIG(TAG, "  Aging offset: %d", this->aging_offset_);
  }
  if (this->monitor_alarms_) {
    ESP_LOGCONFIG(TAG, "  Alarm check interval: %" PRIu32 " ms", this->alarm_check_interval_);
  }
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Alarm 1", this->alarm_1_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Alarm 2", this->alarm_2_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Oscillator Stopped", this->oscillator_stopped_binary_sensor_);
#endif
}

// ---------------------------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------------------------

bool DS3231Component::read_time(ESPTime &out) {
  uint8_t raw[7];
  if (this->is_failed() || !this->read_bytes(DS3231_REG_SECONDS, raw, sizeof(raw))) {
    ESP_LOGW(TAG, "Can't read time registers.");
    return false;
  }
  // Without a valid status the oscillator-stop flag is unknown, so the time can't be trusted.
  if (!this->read_control_status_())
    return false;
  this->publish_oscillator_stopped_();
  if ((this->status_reg_ & STATUS_OSF) != 0) {
    ESP_LOGW(TAG, "Oscillator stop flag set, the RTC time is invalid. Write a valid time first.");
    return false;
  }

  uint8_t hour;
  if ((raw[2] & HOUR_12H) != 0) {
    // 12-hour mode, e.g. set by another library: 12 AM is 0, 12 PM is 12.
    hour = bcd_to_uint8(raw[2] & 0x1F) % 12;
    if ((raw[2] & HOUR_PM) != 0)
      hour += 12;
  } else {
    hour = bcd_to_uint8(raw[2] & 0x3F);
  }

  out = ESPTime{
      .second = bcd_to_uint8(raw[0] & 0x7F),
      .minute = bcd_to_uint8(raw[1] & 0x7F),
      .hour = hour,
      .day_of_week = static_cast<uint8_t>(raw[3] & 0x07),
      .day_of_month = bcd_to_uint8(raw[4] & 0x3F),
      .month = bcd_to_uint8(raw[5] & 0x1F),
      .year = static_cast<uint16_t>(2000 + bcd_to_uint8(raw[6]) + ((raw[5] & MONTH_CENTURY) != 0 ? 100 : 0)),
  };
  out.recalc_timestamp_utc(false);
  ESP_LOGD(TAG, "Read UTC time: %04u-%02u-%02u %02u:%02u:%02u (day of week %u)", out.year, out.month, out.day_of_month,
           out.hour, out.minute, out.second, out.day_of_week);
  // The day of week is a free-running counter on the DS3231 and may have been set with a different
  // numbering by other software, so it is not used to validate the time.
  if (!out.is_valid(/*check_day_of_week=*/false, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time.");
    return false;
  }
  return true;
}

bool DS3231Component::write_time(const ESPTime &time) {
  if (this->is_failed())
    return false;
  if (time.year < 2000 || time.year > 2199) {
    ESP_LOGE(TAG, "Year %u is out of the DS3231 range (2000-2199).", time.year);
    return false;
  }
  const uint16_t year = time.year - 2000;
  uint8_t raw[7];
  raw[0] = uint8_to_bcd(time.second);
  raw[1] = uint8_to_bcd(time.minute);
  raw[2] = uint8_to_bcd(time.hour);  // 24-hour mode
  raw[3] = time.day_of_week;         // 1 = Sunday, as in ESPTime
  raw[4] = uint8_to_bcd(time.day_of_month);
  raw[5] = uint8_to_bcd(time.month) | (year >= 100 ? MONTH_CENTURY : 0);
  raw[6] = uint8_to_bcd(year % 100);
  if (!this->write_bytes(DS3231_REG_SECONDS, raw, sizeof(raw))) {
    ESP_LOGW(TAG, "Can't write time registers.");
    return false;
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04u-%02u-%02u %02u:%02u:%02u", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  // The time is valid again: clear the oscillator stop flag. If that fails, the RTC would keep
  // reporting its time as invalid, so the write is not reported as successful.
  if (!this->read_control_status_())
    return false;
  if ((this->status_reg_ & STATUS_OSF) != 0) {
    if (!this->write_status_(STATUS_OSF))
      return false;
    this->status_reg_ &= ~STATUS_OSF;
  }
  this->publish_oscillator_stopped_();
  return true;
}

// ---------------------------------------------------------------------------------------------
// Temperature
// ---------------------------------------------------------------------------------------------

bool DS3231Component::read_temperature(float &out) {
  uint8_t raw[2];
  if (this->is_failed() || !this->read_bytes(DS3231_REG_TEMPERATURE, raw, sizeof(raw))) {
    return false;
  }
  // 10-bit two's complement, 0.25 degC per LSB.
  int16_t value = encode_value<int16_t>(raw) >> 6;
  out = value * 0.25f;
  return true;
}

void DS3231Component::force_temperature_conversion() {
  // Just kick off the conversion - the chip clears CONV and updates the temperature
  // register on its own within ~200 ms, and the next temperature poll picks it up.
  // Blocking here to wait for it would stall the main loop.
  if (this->is_failed() || !this->read_control_status_())
    return;
  if ((this->status_reg_ & STATUS_BSY) != 0) {
    ESP_LOGW(TAG, "A temperature conversion is already in progress.");
    return;
  }
  this->control_reg_ |= CONTROL_CONV;
  this->write_control_();
  // CONV is a one-shot command bit: don't keep it in the cached value.
  this->control_reg_ &= ~CONTROL_CONV;
}

// ---------------------------------------------------------------------------------------------
// Alarms
// ---------------------------------------------------------------------------------------------

void DS3231Component::set_alarm_1(AlarmMode mode, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day_of_month,
                                  uint8_t day_of_week, bool enabled) {
  if (second > 59 || minute > 59 || hour > 23 || day_of_month < 1 || day_of_month > 31 || day_of_week < 1 ||
      day_of_week > 7 || mode > ALARM_MODE_WEEKLY) {
    ESP_LOGE(TAG, "Invalid alarm 1 setting.");
    return;
  }
  // Mask bits A1M1..A1M4: a set bit means the register is ignored when matching.
  uint8_t regs[4] = {
      uint8_to_bcd(second),
      uint8_to_bcd(minute),
      uint8_to_bcd(hour),
      mode == ALARM_MODE_WEEKLY ? static_cast<uint8_t>(ALARM_DAY_OF_WEEK | day_of_week) : uint8_to_bcd(day_of_month),
  };
  switch (mode) {
    case ALARM_MODE_EVERY_SECOND:
      regs[0] |= ALARM_MASK_BIT;
      [[fallthrough]];
    case ALARM_MODE_EVERY_MINUTE:
      regs[1] |= ALARM_MASK_BIT;
      [[fallthrough]];
    case ALARM_MODE_HOURLY:
      regs[2] |= ALARM_MASK_BIT;
      [[fallthrough]];
    case ALARM_MODE_DAILY:
      regs[3] |= ALARM_MASK_BIT;
      break;
    default:  // MONTHLY / WEEKLY: everything matches
      break;
  }
  ESP_LOGD(TAG, "Setting alarm 1: mode %u, %02u:%02u:%02u, day of month %u, day of week %u, %s", mode, hour, minute,
           second, day_of_month, day_of_week, enabled ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));
  this->write_alarm_(1, regs, sizeof(regs), enabled);
}

void DS3231Component::set_alarm_2(AlarmMode mode, uint8_t minute, uint8_t hour, uint8_t day_of_month,
                                  uint8_t day_of_week, bool enabled) {
  if (minute > 59 || hour > 23 || day_of_month < 1 || day_of_month > 31 || day_of_week < 1 || day_of_week > 7 ||
      mode == ALARM_MODE_EVERY_SECOND || mode > ALARM_MODE_WEEKLY) {
    ESP_LOGE(TAG, "Invalid alarm 2 setting.");
    return;
  }
  // Mask bits A2M2..A2M4.
  uint8_t regs[3] = {
      uint8_to_bcd(minute),
      uint8_to_bcd(hour),
      mode == ALARM_MODE_WEEKLY ? static_cast<uint8_t>(ALARM_DAY_OF_WEEK | day_of_week) : uint8_to_bcd(day_of_month),
  };
  switch (mode) {
    case ALARM_MODE_EVERY_MINUTE:
      regs[0] |= ALARM_MASK_BIT;
      [[fallthrough]];
    case ALARM_MODE_HOURLY:
      regs[1] |= ALARM_MASK_BIT;
      [[fallthrough]];
    case ALARM_MODE_DAILY:
      regs[2] |= ALARM_MASK_BIT;
      break;
    default:  // MONTHLY / WEEKLY: everything matches
      break;
  }
  ESP_LOGD(TAG, "Setting alarm 2: mode %u, %02u:%02u, day of month %u, day of week %u, %s", mode, hour, minute,
           day_of_month, day_of_week, enabled ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));
  this->write_alarm_(2, regs, sizeof(regs), enabled);
}

void DS3231Component::write_alarm_(uint8_t alarm, const uint8_t *regs, size_t len, bool enabled) {
  if (this->is_failed())
    return;
  if (!this->write_bytes(alarm == 1 ? DS3231_REG_ALARM_1 : DS3231_REG_ALARM_2, regs, len)) {
    ESP_LOGW(TAG, "Can't write alarm %u registers.", alarm);
    return;
  }
  // Changing the registers may have matched on the way: start from a clean flag. Enabling the
  // alarm with a stale flag would assert INT/SQW right away.
  if (!this->clear_alarm_flag_(alarm)) {
    ESP_LOGW(TAG, "Alarm %u written, but its flag could not be cleared; not changing its enable.", alarm);
    return;
  }
  this->set_alarm_enabled_(alarm, enabled);
}

void DS3231Component::set_alarm_enabled_(uint8_t alarm, bool enabled) {
  if (alarm != 1 && alarm != 2) {
    ESP_LOGE(TAG, "Invalid alarm number %u.", alarm);
    return;
  }
  if (this->is_failed() || !this->read_control_status_())
    return;
  const uint8_t bit = alarm == 1 ? CONTROL_A1IE : CONTROL_A2IE;
  if (enabled) {
    this->control_reg_ |= bit;
    if ((this->control_reg_ & CONTROL_INTCN) == 0) {
      ESP_LOGW(TAG, "Alarm %u enabled, but INT/SQW is a square-wave output: only the flag will be set.", alarm);
    }
  } else {
    this->control_reg_ &= ~bit;
  }
  if (this->write_control_()) {
    ESP_LOGD(TAG, "Alarm %u %s.", alarm, enabled ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));
  }
}

void DS3231Component::clear_alarm(uint8_t alarm) { this->clear_alarm_flag_(alarm); }

bool DS3231Component::clear_alarm_flag_(uint8_t alarm) {
  if (alarm != 1 && alarm != 2) {
    ESP_LOGE(TAG, "Invalid alarm number %u.", alarm);
    return false;
  }
  if (this->is_failed() || !this->read_control_status_())
    return false;
  const uint8_t flag = alarm == 1 ? STATUS_A1F : STATUS_A2F;
  if ((this->status_reg_ & flag) != 0) {
    // On failure the flag is still set on the chip: keep reporting the alarm as it is.
    if (!this->write_status_(flag))
      return false;
    this->status_reg_ &= ~flag;
  }
  this->alarm_active_[alarm - 1] = false;
  this->publish_alarm_state_(alarm, false);
  return true;
}

bool DS3231Component::is_alarm_triggered(uint8_t alarm) {
  if (this->is_failed() || !this->read_control_status_())
    return false;
  return (this->status_reg_ & (alarm == 1 ? STATUS_A1F : STATUS_A2F)) != 0;
}

void DS3231Component::check_alarms_() {
  if (this->is_failed() || !this->read_control_status_())
    return;
  this->publish_oscillator_stopped_();

  uint8_t to_clear = 0;
  for (uint8_t alarm = 1; alarm <= 2; alarm++) {
    const uint8_t flag = alarm == 1 ? STATUS_A1F : STATUS_A2F;
    const uint8_t enable = alarm == 1 ? CONTROL_A1IE : CONTROL_A2IE;
    const bool triggered = (this->status_reg_ & flag) != 0;
    const bool enabled = (this->control_reg_ & enable) != 0;
    bool &active = this->alarm_active_[alarm - 1];

    if (!triggered) {
      if (active && !this->auto_clear_alarms_)
        this->publish_alarm_state_(alarm, false);
      active = false;
      continue;
    }
    // The flag is set on every match even when the alarm is disabled; only enabled alarms count.
    if (enabled && !active) {
      ESP_LOGI(TAG, "Alarm %u triggered.", alarm);
      active = true;
      this->publish_alarm_state_(alarm, true);
      (alarm == 1 ? this->alarm_1_trigger_ : this->alarm_2_trigger_).trigger();
    }
    if (this->auto_clear_alarms_)
      to_clear |= flag;
  }

  if (to_clear != 0 && this->write_status_(to_clear)) {
    this->status_reg_ &= ~to_clear;
    for (uint8_t alarm = 1; alarm <= 2; alarm++) {
      if ((to_clear & (alarm == 1 ? STATUS_A1F : STATUS_A2F)) == 0 || !this->alarm_active_[alarm - 1])
        continue;
      this->alarm_active_[alarm - 1] = false;
#ifdef USE_BINARY_SENSOR
      // Keep the binary sensor on long enough for the state to be seen, then turn it off again.
      this->set_timeout(static_cast<uint32_t>(alarm), ALARM_PULSE_MS,
                        [this, alarm]() { this->publish_alarm_state_(alarm, false); });
#endif
    }
  }
}

void DS3231Component::publish_alarm_state_(uint8_t alarm, bool state) {
#ifdef USE_BINARY_SENSOR
  auto *sensor = alarm == 1 ? this->alarm_1_binary_sensor_ : this->alarm_2_binary_sensor_;
  if (sensor != nullptr)
    sensor->publish_state(state);
#endif
}

void DS3231Component::publish_oscillator_stopped_() {
#ifdef USE_BINARY_SENSOR
  if (this->oscillator_stopped_binary_sensor_ != nullptr)
    this->oscillator_stopped_binary_sensor_->publish_state((this->status_reg_ & STATUS_OSF) != 0);
#endif
}

// ---------------------------------------------------------------------------------------------
// Outputs and calibration
// ---------------------------------------------------------------------------------------------

void DS3231Component::enable_square_wave(SquareWaveFrequency frequency) {
  if (this->interrupt_pin_ != nullptr) {
    ESP_LOGE(TAG, "Can't enable the square wave: INT/SQW is used as the alarm interrupt pin.");
    return;
  }
  if (this->is_failed() || !this->read_control_status_())
    return;
  this->control_reg_ &= ~(CONTROL_INTCN | CONTROL_RS_MASK);
  this->control_reg_ |= (frequency << CONTROL_RS_SHIFT) & CONTROL_RS_MASK;
  if (this->write_control_()) {
    ESP_LOGD(TAG, "Square wave enabled at %s.", LOG_STR_ARG(square_wave_to_string(frequency)));
  }
}

void DS3231Component::disable_square_wave() {
  if (this->is_failed() || !this->read_control_status_())
    return;
  this->control_reg_ |= CONTROL_INTCN;
  if (this->write_control_()) {
    ESP_LOGD(TAG, "Square wave disabled, INT/SQW is the alarm interrupt.");
  }
}

void DS3231Component::set_output_32khz(bool enabled) {
  if (this->is_failed() || !this->read_control_status_())
    return;
  if (enabled) {
    this->status_reg_ |= STATUS_EN32KHZ;
  } else {
    this->status_reg_ &= ~STATUS_EN32KHZ;
  }
  if (this->write_status_(0)) {
    ESP_LOGD(TAG, "32 kHz output %s.", ONOFF(enabled));
  }
}

void DS3231Component::set_aging_offset(int8_t offset) {
  if (this->is_failed())
    return;
  if (!this->write_byte(DS3231_REG_AGING_OFFSET, static_cast<uint8_t>(offset))) {
    ESP_LOGW(TAG, "Can't write aging offset.");
    return;
  }
  this->aging_offset_ = offset;
  ESP_LOGD(TAG, "Aging offset set to %d.", offset);
  // The new offset takes effect with the next temperature conversion.
  this->force_temperature_conversion();
}

// ---------------------------------------------------------------------------------------------
// Register access
// ---------------------------------------------------------------------------------------------

bool DS3231Component::read_control_status_() {
  uint8_t raw[2];
  if (!this->read_bytes(DS3231_REG_CONTROL, raw, sizeof(raw))) {
    ESP_LOGW(TAG, "Can't read control/status registers.");
    return false;
  }
  this->control_reg_ = raw[0] & ~CONTROL_CONV;
  this->status_reg_ = raw[1];
  return true;
}

bool DS3231Component::write_control_() {
  if (!this->write_byte(DS3231_REG_CONTROL, this->control_reg_)) {
    ESP_LOGW(TAG, "Can't write control register.");
    return false;
  }
  return true;
}

bool DS3231Component::write_status_(uint8_t clear) {
  // The flags can only be written to 0; writing 1 leaves them unchanged.
  const uint8_t value = ((this->status_reg_ & STATUS_EN32KHZ) | STATUS_FLAGS) & ~clear;
  if (!this->write_byte(DS3231_REG_STATUS, value)) {
    ESP_LOGW(TAG, "Can't write status register.");
    return false;
  }
  return true;
}

}  // namespace esphome::ds3231
