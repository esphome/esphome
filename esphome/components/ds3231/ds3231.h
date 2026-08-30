#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"
#include "esphome/components/i2c/i2c.h"
// Base entity headers, only when that entity type is used anywhere in the config. The hub
// keeps base-class pointers to the ds3231 platform entities (the platforms upcast) so a
// config that never uses one of these platforms does not need it compiled or linked.
#ifdef USE_DS3231_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_DS3231_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_DS3231_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome::ds3231 {

#ifdef USE_DS3231_SQUARE_WAVE
/// Frequency of the signal on the INT/SQW pin when it is configured as a square-wave output.
enum class DS3231SquareWaveFrequency : uint8_t {
  DS3231_SQUARE_WAVE_FREQUENCY_1_HZ = 0b00,
  DS3231_SQUARE_WAVE_FREQUENCY_1024_HZ = 0b01,
  DS3231_SQUARE_WAVE_FREQUENCY_4096_HZ = 0b10,
  DS3231_SQUARE_WAVE_FREQUENCY_8192_HZ = 0b11,
};
#endif

#ifdef USE_DS3231_ALARM
/// Match condition for alarm 1 (has one-second resolution).
enum class DS3231Alarm1Mode : uint8_t {
  DS3231_ALARM_1_MODE_EVERY_SECOND,
  DS3231_ALARM_1_MODE_MATCH_SECOND,
  DS3231_ALARM_1_MODE_MATCH_MINUTE_SECOND,
  DS3231_ALARM_1_MODE_MATCH_HOUR_MINUTE_SECOND,
  DS3231_ALARM_1_MODE_MATCH_DAY_OF_MONTH,
  DS3231_ALARM_1_MODE_MATCH_DAY_OF_WEEK,
};

/// Match condition for alarm 2 (has one-minute resolution, always fires at second 0).
enum class DS3231Alarm2Mode : uint8_t {
  DS3231_ALARM_2_MODE_EVERY_MINUTE,
  DS3231_ALARM_2_MODE_MATCH_MINUTE,
  DS3231_ALARM_2_MODE_MATCH_HOUR_MINUTE,
  DS3231_ALARM_2_MODE_MATCH_DAY_OF_MONTH,
  DS3231_ALARM_2_MODE_MATCH_DAY_OF_WEEK,
};

/// Fields used to program an alarm. Unused fields (per the selected mode) are ignored.
struct DS3231AlarmSpec {
  uint8_t second{0};
  uint8_t minute{0};
  uint8_t hour{0};
  uint8_t day{1};
};
#endif

/// Hub for a single DS3231 real-time clock. Owns all register access; the time, sensor,
/// binary_sensor, switch, select and number platforms talk to the chip through this class.
///
/// Optional features are gated behind USE_DS3231_* so a config that only reads the time
/// does not pay for alarm callbacks, the square-wave control, etc.
class DS3231Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // --- Time ----------------------------------------------------------------
  bool read_datetime(ESPTime &out);
  bool write_datetime(const ESPTime &time);

  // --- Temperature -------------------------------------------------------
  bool read_temperature(float &out);
  void force_temperature_conversion();

  bool get_oscillator_stopped() const { return (this->status_reg_ & STATUS_OSF) != 0; }
#ifdef USE_DS3231_BINARY_SENSOR
  void set_oscillator_stopped_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->oscillator_stopped_binary_sensor_ = sensor;
  }
#endif

#ifdef USE_DS3231_REFRESH_INTERVAL
  /// Change the poll interval (how often the alarm flags / oscillator-stop flag are read and
  /// the switch/select entities are refreshed) at runtime. Re-arms the poller.
  void set_refresh_interval(uint32_t interval_ms) {
    this->stop_poller();
    this->set_update_interval(interval_ms);
    this->start_poller();
  }
#endif

#ifdef USE_DS3231_ALARM
  // --- Alarms ----------------------------------------------------------------
  bool set_alarm_1(DS3231Alarm1Mode mode, const DS3231AlarmSpec &spec);
  bool set_alarm_2(DS3231Alarm2Mode mode, const DS3231AlarmSpec &spec);

  /// Read the alarm currently programmed into the chip back into mode and spec.
  /// Returns false on a bus error or if the registers hold an unrecognized mask pattern
  /// (for example on a fresh chip whose alarm registers have never been written).
  bool get_alarm_1(DS3231Alarm1Mode &mode, DS3231AlarmSpec &spec);
  bool get_alarm_2(DS3231Alarm2Mode &mode, DS3231AlarmSpec &spec);

  /// Write a short human-readable summary of the programmed alarm into buf, e.g.
  /// "daily at 07:30:00". Returns false (buf untouched) if the alarm cannot be read.
  bool describe_alarm_1(char *buf, size_t len);
  bool describe_alarm_2(char *buf, size_t len);

  bool set_alarm_enabled(uint8_t alarm, bool enabled);
  bool clear_alarm(uint8_t alarm);

  template<typename F> void add_on_alarm_1_callback(F &&callback) {
    this->alarm_1_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_alarm_2_callback(F &&callback) {
    this->alarm_2_callback_.add(std::forward<F>(callback));
  }

  /// Whether the alarm interrupt is enabled (A1IE / A2IE). set_alarm_1 / set_alarm_2 enable it;
  /// clear_alarm only clears the fired flag and leaves the alarm enabled.
  bool get_alarm_1_enabled() const { return (this->control_reg_ & CONTROL_A1IE) != 0; }
  bool get_alarm_2_enabled() const { return (this->control_reg_ & CONTROL_A2IE) != 0; }

#ifdef USE_DS3231_BINARY_SENSOR
  void set_alarm_1_binary_sensor(binary_sensor::BinarySensor *sensor) { this->alarm_1_binary_sensor_ = sensor; }
  void set_alarm_2_binary_sensor(binary_sensor::BinarySensor *sensor) { this->alarm_2_binary_sensor_ = sensor; }
#endif
#ifdef USE_DS3231_SWITCH
  void set_alarm_1_switch(switch_::Switch *sw) { this->alarm_1_switch_ = sw; }
  void set_alarm_2_switch(switch_::Switch *sw) { this->alarm_2_switch_ = sw; }
#endif
#endif  // USE_DS3231_ALARM

#ifdef USE_DS3231_SQUARE_WAVE
  // --- Square-wave output ------------------------------------------------
  void set_square_wave_output(DS3231SquareWaveFrequency frequency) {
    this->square_wave_frequency_ = frequency;
    this->square_wave_output_ = true;
  }
  void set_battery_backed_square_wave(bool enabled) { this->battery_backed_square_wave_ = enabled; }

  /// Route the INT/SQW pin at runtime: true drives the square-wave output at the configured
  /// frequency (default 1 Hz), false makes it the alarm-interrupt line. Writes the control
  /// register. Enabling an alarm also switches the pin back to interrupt mode.
  bool set_square_wave_output_enabled(bool enabled);
  bool get_square_wave_output_enabled() const { return (this->control_reg_ & CONTROL_INTCN) == 0; }

  /// Set the square-wave frequency (also used the next time the square wave is enabled).
  /// Writes the control register; returns I2C success.
  bool set_square_wave_frequency(DS3231SquareWaveFrequency frequency);
  DS3231SquareWaveFrequency get_square_wave_frequency() const {
    return static_cast<DS3231SquareWaveFrequency>((this->control_reg_ >> 3) & 0b11);
  }

#ifdef USE_DS3231_SELECT
  void set_output_mode_select(select::Select *sel) { this->output_mode_select_ = sel; }
  void set_square_wave_frequency_select(select::Select *sel) { this->square_wave_frequency_select_ = sel; }
#endif
#endif  // USE_DS3231_SQUARE_WAVE

#ifdef USE_DS3231_32KHZ_OUTPUT
  bool set_32khz_output(bool enabled);
  bool get_32khz_output() const { return (this->status_reg_ & STATUS_EN32KHZ) != 0; }
#endif

#ifdef USE_DS3231_AGING_OFFSET
  bool set_aging_offset(int8_t offset);
  bool read_aging_offset(int8_t &out);
#endif

 protected:
  static constexpr uint8_t CONTROL_A1IE = 0x01;
  static constexpr uint8_t CONTROL_A2IE = 0x02;
  static constexpr uint8_t CONTROL_INTCN = 0x04;
  static constexpr uint8_t CONTROL_RS1 = 0x08;
  static constexpr uint8_t CONTROL_RS2 = 0x10;
  static constexpr uint8_t CONTROL_CONV = 0x20;
  static constexpr uint8_t CONTROL_BBSQW = 0x40;
  static constexpr uint8_t CONTROL_EOSC = 0x80;

  static constexpr uint8_t STATUS_A1F = 0x01;
  static constexpr uint8_t STATUS_A2F = 0x02;
  static constexpr uint8_t STATUS_BSY = 0x04;
  static constexpr uint8_t STATUS_EN32KHZ = 0x08;
  static constexpr uint8_t STATUS_OSF = 0x80;

  bool read_control_status_();
  bool write_control_();
  bool write_status_();

#ifdef USE_DS3231_ALARM
  void handle_alarm_flags_();
#endif

#ifdef USE_DS3231_SQUARE_WAVE
  /// Set the RS1/RS2 bits in control_reg_ from square_wave_frequency_ (no I2C).
  void apply_square_wave_frequency_bits_() {
    this->control_reg_ &= ~(CONTROL_RS1 | CONTROL_RS2);
    this->control_reg_ |= static_cast<uint8_t>(this->square_wave_frequency_) << 3;
  }

  DS3231SquareWaveFrequency square_wave_frequency_{DS3231SquareWaveFrequency::DS3231_SQUARE_WAVE_FREQUENCY_1_HZ};
  bool square_wave_output_{false};
  bool battery_backed_square_wave_{false};
#endif

  uint8_t control_reg_{0};
  uint8_t status_reg_{0};

#ifdef USE_DS3231_BINARY_SENSOR
  binary_sensor::BinarySensor *oscillator_stopped_binary_sensor_{nullptr};
#endif

#ifdef USE_DS3231_ALARM
  CallbackManager<void()> alarm_1_callback_;
  CallbackManager<void()> alarm_2_callback_;
#ifdef USE_DS3231_BINARY_SENSOR
  binary_sensor::BinarySensor *alarm_1_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *alarm_2_binary_sensor_{nullptr};
#endif
#ifdef USE_DS3231_SWITCH
  switch_::Switch *alarm_1_switch_{nullptr};
  switch_::Switch *alarm_2_switch_{nullptr};
#endif
#endif  // USE_DS3231_ALARM

#if defined(USE_DS3231_SQUARE_WAVE) && defined(USE_DS3231_SELECT)
  select::Select *output_mode_select_{nullptr};
  select::Select *square_wave_frequency_select_{nullptr};
#endif
};

#ifdef USE_DS3231_ALARM
template<typename... Ts> class SetAlarm1Action final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, second)
  TEMPLATABLE_VALUE(uint8_t, minute)
  TEMPLATABLE_VALUE(uint8_t, hour)
  TEMPLATABLE_VALUE(uint8_t, day)

  void set_mode(DS3231Alarm1Mode mode) { this->mode_ = mode; }

  void play(const Ts &...x) override {
    DS3231AlarmSpec spec;
    spec.second = this->second_.value_or(x..., 0);
    spec.minute = this->minute_.value_or(x..., 0);
    spec.hour = this->hour_.value_or(x..., 0);
    spec.day = this->day_.value_or(x..., 1);
    this->parent_->set_alarm_1(this->mode_, spec);
  }

 protected:
  DS3231Alarm1Mode mode_{DS3231Alarm1Mode::DS3231_ALARM_1_MODE_EVERY_SECOND};
};

template<typename... Ts> class SetAlarm2Action final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  TEMPLATABLE_VALUE(uint8_t, minute)
  TEMPLATABLE_VALUE(uint8_t, hour)
  TEMPLATABLE_VALUE(uint8_t, day)

  void set_mode(DS3231Alarm2Mode mode) { this->mode_ = mode; }

  void play(const Ts &...x) override {
    DS3231AlarmSpec spec;
    spec.minute = this->minute_.value_or(x..., 0);
    spec.hour = this->hour_.value_or(x..., 0);
    spec.day = this->day_.value_or(x..., 1);
    this->parent_->set_alarm_2(this->mode_, spec);
  }

 protected:
  DS3231Alarm2Mode mode_{DS3231Alarm2Mode::DS3231_ALARM_2_MODE_EVERY_MINUTE};
};

template<typename... Ts> class ClearAlarmAction final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void set_alarm(uint8_t alarm) { this->alarm_ = alarm; }
  void play(const Ts &...x) override { this->parent_->clear_alarm(this->alarm_); }

 protected:
  uint8_t alarm_{1};
};

/// Enables an alarm's interrupt (sets A1IE / A2IE) without reprogramming its time - the action
/// form of turning the alarm_N switch on. Pair of DisableAlarmAction.
template<typename... Ts> class EnableAlarmAction final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void set_alarm(uint8_t alarm) { this->alarm_ = alarm; }
  void play(const Ts &...x) override { this->parent_->set_alarm_enabled(this->alarm_, true); }

 protected:
  uint8_t alarm_{1};
};

/// Disables an alarm's interrupt (clears A1IE / A2IE) without touching its programmed time, so it
/// can be re-enabled later with enable_alarm, switch.turn_on, or set_alarm_1 / set_alarm_2.
template<typename... Ts> class DisableAlarmAction final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void set_alarm(uint8_t alarm) { this->alarm_ = alarm; }
  void play(const Ts &...x) override { this->parent_->set_alarm_enabled(this->alarm_, false); }

 protected:
  uint8_t alarm_{1};
};
#endif  // USE_DS3231_ALARM

template<typename... Ts>
class ForceTemperatureConversionAction final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(const Ts &...x) override { this->parent_->force_temperature_conversion(); }
};

#ifdef USE_DS3231_REFRESH_INTERVAL
template<typename... Ts> class SetRefreshIntervalAction final : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  TEMPLATABLE_VALUE(uint32_t, refresh_interval)
  void play(const Ts &...x) override { this->parent_->set_refresh_interval(this->refresh_interval_.value(x...)); }
};
#endif

}  // namespace esphome::ds3231
