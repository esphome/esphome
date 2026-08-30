#include "ds3231.h"
#ifdef USE_DS3231_ALARM
#include <cstdio>
#endif
#include "esphome/core/log.h"

// Datasheet:
// - https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf

namespace esphome::ds3231 {

static const char *const TAG = "ds3231";

static const uint8_t DS3231_REG_TIME = 0x00;
static const uint8_t DS3231_REG_CONTROL = 0x0E;
static const uint8_t DS3231_REG_STATUS = 0x0F;
static const uint8_t DS3231_REG_TEMPERATURE = 0x11;
#ifdef USE_DS3231_ALARM
static const uint8_t DS3231_REG_ALARM_1 = 0x07;
static const uint8_t DS3231_REG_ALARM_2 = 0x0B;
#endif
#ifdef USE_DS3231_AGING_OFFSET
static const uint8_t DS3231_REG_AGING_OFFSET = 0x10;
#endif

#ifdef USE_DS3231_SQUARE_WAVE
static const char *const SQUARE_WAVE_FREQUENCY_NAMES[] = {"1 Hz", "1.024 kHz", "4.096 kHz", "8.192 kHz"};
#endif

constexpr uint8_t bcd2dec(uint8_t value) { return (value >> 4) * 10 + (value & 0x0F); }
constexpr uint8_t dec2bcd(uint8_t value) { return ((value / 10) << 4) | (value % 10); }

// Decode an hours register (time or alarm) that may be in 12- or 24-hour form.
static uint8_t decode_hour(uint8_t reg) {
  if ((reg & 0x40) != 0) {  // 12-hour mode
    uint8_t hour = bcd2dec(reg & 0x1F) % 12;
    if ((reg & 0x20) != 0)  // PM
      hour += 12;
    return hour;
  }
  return bcd2dec(reg & 0x3F);
}

void DS3231Component::setup() {
  if (!this->read_control_status_()) {
    this->mark_failed();
    return;
  }

  this->control_reg_ &= ~CONTROL_EOSC;  // keep the oscillator running on battery
#ifdef USE_DS3231_SQUARE_WAVE
  // If a square-wave output is requested we clear INTCN so the pin emits the selected
  // frequency; alarm actions later set INTCN back to route alarms to the pin.
  this->apply_square_wave_frequency_bits_();
  if (this->square_wave_output_) {
    this->control_reg_ &= ~CONTROL_INTCN;
  } else {
    this->control_reg_ |= CONTROL_INTCN;
  }
  if (this->battery_backed_square_wave_) {
    this->control_reg_ |= CONTROL_BBSQW;
  } else {
    this->control_reg_ &= ~CONTROL_BBSQW;
  }
#else
  // Without square-wave support the INT/SQW pin is always the alarm-interrupt line: idle
  // high, pulled low on an alarm match, regardless of how the chip was left before.
  this->control_reg_ |= CONTROL_INTCN;
#endif
  if (!this->write_control_()) {
    this->mark_failed();
    return;
  }

  if (this->get_oscillator_stopped()) {
    ESP_LOGW(TAG, "Oscillator stop flag set - the clock lost power and the time is not reliable.");
  }
}

void DS3231Component::update() {
  if (!this->read_control_status_()) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

#ifdef USE_DS3231_ALARM
  this->handle_alarm_flags_();
#ifdef USE_DS3231_SWITCH
  if (this->alarm_1_switch_ != nullptr) {
    this->alarm_1_switch_->publish_state(this->get_alarm_1_enabled());
  }
  if (this->alarm_2_switch_ != nullptr) {
    this->alarm_2_switch_->publish_state(this->get_alarm_2_enabled());
  }
#endif
#endif

#ifdef USE_DS3231_BINARY_SENSOR
  if (this->oscillator_stopped_binary_sensor_ != nullptr) {
    this->oscillator_stopped_binary_sensor_->publish_state(this->get_oscillator_stopped());
  }
#endif

#if defined(USE_DS3231_SQUARE_WAVE) && defined(USE_DS3231_SELECT)
  // Keep the selects in step with the chip - e.g. arming an alarm forces interrupt mode.
  if (this->output_mode_select_ != nullptr) {
    size_t index = this->get_square_wave_output_enabled() ? 1 : 0;
    if (this->output_mode_select_->active_index() != index) {
      this->output_mode_select_->publish_state(index);
    }
  }
  if (this->square_wave_frequency_select_ != nullptr) {
    auto index = static_cast<size_t>(this->get_square_wave_frequency());
    if (this->square_wave_frequency_select_->active_index() != index) {
      this->square_wave_frequency_select_->publish_state(index);
    }
  }
#endif
}

void DS3231Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

#ifdef USE_DS3231_SQUARE_WAVE
  // INT/SQW pin routing and BBSQW are applied from the config in setup().
  if ((this->control_reg_ & CONTROL_INTCN) != 0) {
    ESP_LOGCONFIG(TAG, "  INT/SQW pin: alarm interrupt");
  } else {
    ESP_LOGCONFIG(TAG, "  INT/SQW pin: square wave (%s)",
                  SQUARE_WAVE_FREQUENCY_NAMES[(this->control_reg_ >> 3) & 0b11]);
  }
  ESP_LOGCONFIG(TAG, "  Battery-backed square wave: %s", YESNO((this->control_reg_ & CONTROL_BBSQW) != 0));
#endif

  if (this->get_oscillator_stopped()) {
    ESP_LOGW(TAG, "  Oscillator stop flag is set (clock lost power)");
  }

#ifdef USE_DS3231_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Oscillator stopped", this->oscillator_stopped_binary_sensor_);
#ifdef USE_DS3231_ALARM
  LOG_BINARY_SENSOR("  ", "Alarm 1", this->alarm_1_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Alarm 2", this->alarm_2_binary_sensor_);
#endif
#endif
#if defined(USE_DS3231_ALARM) && defined(USE_DS3231_SWITCH)
  LOG_SWITCH("  ", "Alarm 1 enabled", this->alarm_1_switch_);
  LOG_SWITCH("  ", "Alarm 2 enabled", this->alarm_2_switch_);
#endif
#if defined(USE_DS3231_SQUARE_WAVE) && defined(USE_DS3231_SELECT)
  LOG_SELECT("  ", "INT/SQW output mode", this->output_mode_select_);
  LOG_SELECT("  ", "Square-wave frequency", this->square_wave_frequency_select_);
#endif
}

bool DS3231Component::read_datetime(ESPTime &out) {
  uint8_t raw[7];
  if (!this->read_bytes(DS3231_REG_TIME, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't read time registers.");
    return false;
  }

  out = ESPTime{
      .second = bcd2dec(raw[0] & 0x7F),
      .minute = bcd2dec(raw[1] & 0x7F),
      .hour = decode_hour(raw[2]),
      .day_of_week = uint8_t(raw[3] & 0x07),
      .day_of_month = bcd2dec(raw[4] & 0x3F),
      .day_of_year = 1,  // recalculated below
      .month = bcd2dec(raw[5] & 0x1F),
      .year = uint16_t(bcd2dec(raw[6]) + ((raw[5] & 0x80) != 0 ? 2100 : 2000)),
  };
  return true;
}

bool DS3231Component::write_datetime(const ESPTime &time) {
  uint8_t raw[7];
  raw[0] = dec2bcd(time.second);
  raw[1] = dec2bcd(time.minute);
  raw[2] = dec2bcd(time.hour);  // always write 24-hour mode
  raw[3] = time.day_of_week;
  raw[4] = dec2bcd(time.day_of_month);
  raw[5] = dec2bcd(time.month) | (time.year >= 2100 ? 0x80 : 0x00);
  raw[6] = dec2bcd(static_cast<uint8_t>((time.year - 2000) % 100));

  if (!this->write_bytes(DS3231_REG_TIME, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't write time registers.");
    return false;
  }

  // Writing a valid time clears the oscillator stop flag.
  if (this->get_oscillator_stopped()) {
    this->status_reg_ &= ~STATUS_OSF;
    this->write_status_();
  }
  return true;
}

bool DS3231Component::read_temperature(float &out) {
  uint8_t raw[2];
  if (!this->read_bytes(DS3231_REG_TEMPERATURE, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't read temperature registers.");
    return false;
  }
  // 10-bit two's complement, 0.25 degC per LSB.
  int16_t value = static_cast<int16_t>((raw[0] << 8) | raw[1]) >> 6;
  out = value * 0.25f;
  return true;
}

void DS3231Component::force_temperature_conversion() {
  // Just kick off the conversion - the chip clears CONV and updates the temperature
  // register on its own within ~200 ms, and the next temperature poll picks it up.
  // Blocking here to wait for it would stall the main loop.
  if (!this->read_control_status_())
    return;
  if ((this->status_reg_ & STATUS_BSY) != 0) {
    ESP_LOGW(TAG, "A temperature conversion is already in progress.");
    return;
  }
  this->control_reg_ |= CONTROL_CONV;
  this->write_control_();
}

bool DS3231Component::read_control_status_() {
  uint8_t raw[2];
  if (!this->read_bytes(DS3231_REG_CONTROL, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't read control/status registers.");
    return false;
  }
  this->control_reg_ = raw[0];
  this->status_reg_ = raw[1];
  return true;
}

bool DS3231Component::write_control_() {
  if (!this->write_byte(DS3231_REG_CONTROL, this->control_reg_)) {
    ESP_LOGE(TAG, "Can't write control register.");
    return false;
  }
  return true;
}

bool DS3231Component::write_status_() {
  if (!this->write_byte(DS3231_REG_STATUS, this->status_reg_)) {
    ESP_LOGE(TAG, "Can't write status register.");
    return false;
  }
  return true;
}

#ifdef USE_DS3231_ALARM

void DS3231Component::handle_alarm_flags_() {
  // The chip sets A1F / A2F whenever the time matches the alarm registers, even when
  // that alarm's interrupt is disabled. Only treat a flag as a real "fired" event
  // when the matching enable bit (A1IE / A2IE) is also set.
  bool a1_flag = (this->status_reg_ & STATUS_A1F) != 0;
  bool a2_flag = (this->status_reg_ & STATUS_A2F) != 0;
  bool a1 = a1_flag && this->get_alarm_1_enabled();
  bool a2 = a2_flag && this->get_alarm_2_enabled();

#ifdef USE_DS3231_BINARY_SENSOR
  if (this->alarm_1_binary_sensor_ != nullptr) {
    this->alarm_1_binary_sensor_->publish_state(a1);
  }
  if (this->alarm_2_binary_sensor_ != nullptr) {
    this->alarm_2_binary_sensor_->publish_state(a2);
  }
#endif

  // Clear whichever flag bits are set - handled or not - so a match that happened
  // while the alarm was disabled does not linger and fire the moment it is enabled.
  if (a1_flag || a2_flag) {
    this->status_reg_ &= ~((a1_flag ? STATUS_A1F : 0) | (a2_flag ? STATUS_A2F : 0));
    this->write_status_();
  }
  if (a1) {
    ESP_LOGD(TAG, "Alarm 1 fired");
    this->alarm_1_callback_.call();
  }
  if (a2) {
    ESP_LOGD(TAG, "Alarm 2 fired");
    this->alarm_2_callback_.call();
  }
}

bool DS3231Component::set_alarm_1(DS3231Alarm1Mode mode, const DS3231AlarmSpec &spec) {
  bool m1 = true, m2 = true, m3 = true, m4 = true, dydt = false;
  switch (mode) {
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_EVERY_SECOND:
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_SECOND:
      m1 = false;
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_MINUTE_SECOND:
      m1 = m2 = false;
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_HOUR_MINUTE_SECOND:
      m1 = m2 = m3 = false;
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_MONTH:
      m1 = m2 = m3 = m4 = false;
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_WEEK:
      m1 = m2 = m3 = m4 = false;
      dydt = true;
      break;
  }

  uint8_t raw[4];
  raw[0] = dec2bcd(spec.second) | (m1 ? 0x80 : 0x00);
  raw[1] = dec2bcd(spec.minute) | (m2 ? 0x80 : 0x00);
  raw[2] = dec2bcd(spec.hour) | (m3 ? 0x80 : 0x00);
  raw[3] = dec2bcd(spec.day) | (m4 ? 0x80 : 0x00) | (dydt ? 0x40 : 0x00);
  if (!this->write_bytes(DS3231_REG_ALARM_1, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't write alarm 1 registers.");
    return false;
  }
  return this->set_alarm_enabled(1, true);
}

bool DS3231Component::set_alarm_2(DS3231Alarm2Mode mode, const DS3231AlarmSpec &spec) {
  bool m2 = true, m3 = true, m4 = true, dydt = false;
  switch (mode) {
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_EVERY_MINUTE:
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_MINUTE:
      m2 = false;
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_HOUR_MINUTE:
      m2 = m3 = false;
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_MONTH:
      m2 = m3 = m4 = false;
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_WEEK:
      m2 = m3 = m4 = false;
      dydt = true;
      break;
  }

  uint8_t raw[3];
  raw[0] = dec2bcd(spec.minute) | (m2 ? 0x80 : 0x00);
  raw[1] = dec2bcd(spec.hour) | (m3 ? 0x80 : 0x00);
  raw[2] = dec2bcd(spec.day) | (m4 ? 0x80 : 0x00) | (dydt ? 0x40 : 0x00);
  if (!this->write_bytes(DS3231_REG_ALARM_2, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't write alarm 2 registers.");
    return false;
  }
  return this->set_alarm_enabled(2, true);
}

bool DS3231Component::get_alarm_1(DS3231Alarm1Mode &mode, DS3231AlarmSpec &spec) {
  uint8_t raw[4];
  if (!this->read_bytes(DS3231_REG_ALARM_1, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't read alarm 1 registers.");
    return false;
  }
  // The A1M1..A1M4 mask bits (bit 7 of each register) form a thermometer code that maps
  // one-to-one onto the modes; anything else means the registers were never programmed.
  bool dydt = (raw[3] & 0x40) != 0;
  uint8_t mask = ((raw[3] & 0x80) >> 4) | ((raw[2] & 0x80) >> 5) | ((raw[1] & 0x80) >> 6) | ((raw[0] & 0x80) >> 7);
  switch (mask) {
    case 0b1111:
      mode = DS3231Alarm1Mode::DS3231_ALARM_1_MODE_EVERY_SECOND;
      break;
    case 0b1110:
      mode = DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_SECOND;
      break;
    case 0b1100:
      mode = DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_MINUTE_SECOND;
      break;
    case 0b1000:
      mode = DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_HOUR_MINUTE_SECOND;
      break;
    case 0b0000:
      mode = dydt ? DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_WEEK
                  : DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_MONTH;
      break;
    default:
      ESP_LOGW(TAG, "Alarm 1 registers hold an unrecognized mask pattern (0x%02X).", mask);
      return false;
  }
  spec.second = bcd2dec(raw[0] & 0x7F);
  spec.minute = bcd2dec(raw[1] & 0x7F);
  spec.hour = decode_hour(raw[2] & 0x7F);
  spec.day = dydt ? uint8_t(raw[3] & 0x0F) : bcd2dec(raw[3] & 0x3F);
  return true;
}

bool DS3231Component::get_alarm_2(DS3231Alarm2Mode &mode, DS3231AlarmSpec &spec) {
  uint8_t raw[3];
  if (!this->read_bytes(DS3231_REG_ALARM_2, raw, sizeof(raw))) {
    ESP_LOGE(TAG, "Can't read alarm 2 registers.");
    return false;
  }
  bool dydt = (raw[2] & 0x40) != 0;
  uint8_t mask = ((raw[2] & 0x80) >> 5) | ((raw[1] & 0x80) >> 6) | ((raw[0] & 0x80) >> 7);
  switch (mask) {
    case 0b111:
      mode = DS3231Alarm2Mode::DS3231_ALARM_2_MODE_EVERY_MINUTE;
      break;
    case 0b110:
      mode = DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_MINUTE;
      break;
    case 0b100:
      mode = DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_HOUR_MINUTE;
      break;
    case 0b000:
      mode = dydt ? DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_WEEK
                  : DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_MONTH;
      break;
    default:
      ESP_LOGW(TAG, "Alarm 2 registers hold an unrecognized mask pattern (0x%02X).", mask);
      return false;
  }
  spec.second = 0;  // alarm 2 always matches at second 0
  spec.minute = bcd2dec(raw[0] & 0x7F);
  spec.hour = decode_hour(raw[1] & 0x7F);
  spec.day = dydt ? uint8_t(raw[2] & 0x0F) : bcd2dec(raw[2] & 0x3F);
  return true;
}

bool DS3231Component::describe_alarm_1(char *buf, size_t len) {
  DS3231Alarm1Mode mode;
  DS3231AlarmSpec s;
  if (!this->get_alarm_1(mode, s))
    return false;
  switch (mode) {
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_EVERY_SECOND:
      snprintf(buf, len, "every second");
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_SECOND:
      snprintf(buf, len, "every minute at :%02d", s.second);
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_MINUTE_SECOND:
      snprintf(buf, len, "every hour at %02d:%02d", s.minute, s.second);
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_HOUR_MINUTE_SECOND:
      snprintf(buf, len, "daily at %02d:%02d:%02d", s.hour, s.minute, s.second);
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_MONTH:
      snprintf(buf, len, "day %d at %02d:%02d:%02d", s.day, s.hour, s.minute, s.second);
      break;
    case DS3231Alarm1Mode::DS3231_ALARM_1_MODE_MATCH_DAY_OF_WEEK:
      snprintf(buf, len, "weekday %d at %02d:%02d:%02d", s.day, s.hour, s.minute, s.second);
      break;
  }
  return true;
}

bool DS3231Component::describe_alarm_2(char *buf, size_t len) {
  DS3231Alarm2Mode mode;
  DS3231AlarmSpec s;
  if (!this->get_alarm_2(mode, s))
    return false;
  switch (mode) {
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_EVERY_MINUTE:
      snprintf(buf, len, "every minute");
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_MINUTE:
      snprintf(buf, len, "every hour at %02d:00", s.minute);
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_HOUR_MINUTE:
      snprintf(buf, len, "daily at %02d:%02d", s.hour, s.minute);
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_MONTH:
      snprintf(buf, len, "day %d at %02d:%02d", s.day, s.hour, s.minute);
      break;
    case DS3231Alarm2Mode::DS3231_ALARM_2_MODE_MATCH_DAY_OF_WEEK:
      snprintf(buf, len, "weekday %d at %02d:%02d", s.day, s.hour, s.minute);
      break;
  }
  return true;
}

bool DS3231Component::set_alarm_enabled(uint8_t alarm, bool enabled) {
  if (!this->read_control_status_())
    return false;
  uint8_t bit = alarm == 1 ? CONTROL_A1IE : CONTROL_A2IE;
  if (enabled) {
    this->control_reg_ |= bit | CONTROL_INTCN;
  } else {
    this->control_reg_ &= ~bit;
  }
  return this->write_control_();
}

bool DS3231Component::clear_alarm(uint8_t alarm) {
  if (!this->read_control_status_())
    return false;
  this->status_reg_ &= ~(alarm == 1 ? STATUS_A1F : STATUS_A2F);
  return this->write_status_();
}

#endif  // USE_DS3231_ALARM

#ifdef USE_DS3231_32KHZ_OUTPUT
bool DS3231Component::set_32khz_output(bool enabled) {
  if (!this->read_control_status_())
    return false;
  if (enabled) {
    this->status_reg_ |= STATUS_EN32KHZ;
  } else {
    this->status_reg_ &= ~STATUS_EN32KHZ;
  }
  return this->write_status_();
}
#endif

#ifdef USE_DS3231_SQUARE_WAVE
bool DS3231Component::set_square_wave_output_enabled(bool enabled) {
  if (!this->read_control_status_())
    return false;
  if (enabled) {
    this->apply_square_wave_frequency_bits_();
    this->control_reg_ &= ~CONTROL_INTCN;  // route the pin to the square wave
  } else {
    this->control_reg_ |= CONTROL_INTCN;  // route the pin to the alarm interrupt
  }
  return this->write_control_();
}

bool DS3231Component::set_square_wave_frequency(DS3231SquareWaveFrequency frequency) {
  this->square_wave_frequency_ = frequency;
  if (!this->read_control_status_())
    return false;
  this->apply_square_wave_frequency_bits_();
  return this->write_control_();
}
#endif  // USE_DS3231_SQUARE_WAVE

#ifdef USE_DS3231_AGING_OFFSET
bool DS3231Component::set_aging_offset(int8_t offset) {
  if (!this->write_byte(DS3231_REG_AGING_OFFSET, static_cast<uint8_t>(offset))) {
    ESP_LOGE(TAG, "Can't write aging offset register.");
    return false;
  }
  // A new aging offset only takes effect on the next temperature conversion.
  this->force_temperature_conversion();
  return true;
}

bool DS3231Component::read_aging_offset(int8_t &out) {
  uint8_t value;
  if (!this->read_byte(DS3231_REG_AGING_OFFSET, &value)) {
    ESP_LOGE(TAG, "Can't read aging offset register.");
    return false;
  }
  out = static_cast<int8_t>(value);
  return true;
}
#endif  // USE_DS3231_AGING_OFFSET

}  // namespace esphome::ds3231
